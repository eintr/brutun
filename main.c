/********************************************************
 *  Sorry, no garbage infomations here.
 *  Lisence: read the LISENCE file,
 *  Author(s): read the AUTHOR file and the git commit log,
 *  Code history: read the git commit log.
 *  That's all.
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <dlfcn.h>
#include <pthread.h>

#include <linux/if.h>
#include <linux/if_tun.h>

#include "util_cjson.h"
#include "carrier_interface.h"

#define	CMDSIZE	1024
#define	PKTSIZE	65536

int hup_notified=0;

static carrier_interface_t *carrier = NULL;
static void *carrier_handler=NULL;
static void *carrier_ctx=NULL;
static int loop=1;
static int tun_fd=-1;

static int tun_alloc(char *dev) {

	struct ifreq ifr;
	int fd, err;

	/* Arguments taken by the function:
	 *
	 * char *dev: the name of an interface (or '\0'). MUST have enough
	 *   space to hold the interface name if '\0' is passed
	 * int flags: interface flags (eg, IFF_TUN etc.)
	 */

	/* open the clone device */
	if( (fd = open("/dev/net/tun", O_RDWR)) < 0 ) {
		return fd;
	}

	/* preparation of the struct ifr, of type "struct ifreq" */
	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_flags = IFF_TUN | IFF_NO_PI;   /* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

	if (*dev) {
		/* if a device name was specified, put it in the structure; otherwise,
		 * the kernel will try to allocate the "next" device of the
		 * specified type */
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}

	/* try to create the device */
	if ( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
		close(fd);
		return err;
	}

	/* if the operation was successful, write back the name of the
	 * interface to the variable "dev", so the caller can know
	 * it. Note that the caller MUST reserve space in *dev (see calling
	 * code below) */
	strcpy(dev, ifr.ifr_name);

	/* this is the special file descriptor that the caller will use to talk
	 * with the virtual interface */
	return fd;
}

static int shell(const char *fmt, ...)
 {
       const size_t max_cmdlen = 128*1024;
       int ret;
       char *cmd;

       cmd = malloc(max_cmdlen);
       {
               va_list al;
               va_start(al, fmt);
               vsnprintf(cmd, max_cmdlen-1, fmt, al);
               cmd[max_cmdlen-1] = 0;
               va_end(al);
       }
	fprintf(stderr, "run: %s  ...  ", cmd);
	ret = system(cmd);
	if (ret==-1) {
		fprintf(stderr, "failed: %m.\n");
	} else {
		fprintf(stderr, "status=%d.\n", ret);
	}
	return ret;
}

static void *thr_tun_reader(void *p)
{
	char buffer[PKTSIZE];
	int len;

	while(loop) {
		len = read(tun_fd, buffer, PKTSIZE);
		if (len<=0) {
			continue;
		}
		carrier->send_packet(carrier_ctx, buffer, len);
	}
	pthread_exit(NULL);
}

static void hup_handler(int s)
{
	hup_notified = 1;
}

static void sig_exit(int s)
{
	loop = 0;
}

static void cb_recv(const void *data, size_t len)
{
	ssize_t ret = write(tun_fd, data, len);
	if (len!=ret) {
		fprintf(stderr, "write(tun) incomplete: %m\n");
	}
}

int
main(int argc, char **argv)
{
	char tun_name[IFNAMSIZ];
	cJSON *conf;
	const cJSON *routes;
	const char *tun_local_addr, *tun_peer_addr, *default_route;
	pthread_t tid_tun_reader;

	if (argc<2) {
		fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
		abort();
	}

	srand(getpid());

	conf = cJSON_loadfile(argv[1]);
	if (conf==NULL) {
		fprintf(stderr, "Load config failed.\n");
		exit(1);
	}

	const char *plugin_path;
	plugin_path = cJSON_lookup_str(conf, ".Carrier.Module", NULL);
	if (plugin_path==NULL) {
		fprintf(stderr, "Undefined: .Carrier.Module\n");
		abort();
	}
	fprintf(stderr, "Loading: %s\n", plugin_path);

	carrier_handler = dlopen(plugin_path, RTLD_NOW);
	if (carrier_handler==NULL) {
		fprintf(stderr, "Open plugin %s failed: %s\n", plugin_path, dlerror());
		exit(1);
	}
	carrier = dlsym(carrier_handler, "carrier_interface");
	if (carrier==NULL) {
		fprintf(stderr, "%s seems not a carrier plugin!\n", plugin_path);
		exit(1);
	}

	carrier_ctx = carrier->init(cJSON_lookup_obj(conf, ".Carrier.Config", NULL));
	if (carrier_ctx==NULL) {
		fprintf(stderr, "Failed to init plugin!\n");
		exit(1);
	}
	carrier->on_packet_receive(carrier_ctx, cb_recv);

	fprintf(stderr, "Inited plugin: %s\n", carrier->name);

	signal(SIGTERM, sig_exit);
	signal(SIGINT, sig_exit);
	signal(SIGQUIT, sig_exit);
	signal(SIGHUP, hup_handler);

	tun_local_addr = cJSON_lookup_str(conf, ".TunnelLocalAddr", NULL);
	tun_peer_addr = cJSON_lookup_str(conf, ".TunnelPeerAddr", NULL);
	if (tun_local_addr==NULL || tun_peer_addr==NULL) {
		fprintf(stderr, "Must define TunnelLocalAddr and TunnelPeerAddr in config file!\n");
		exit(1);
	}

	tun_name[0]='\0';
	tun_fd = tun_alloc(tun_name);
	if (tun_fd<0) {
		perror("tun_alloc()");
		exit(1);
	}

	shell("ip addr add dev %s %s peer %s", tun_name, tun_local_addr, tun_peer_addr);
	shell("ip link set dev %s up", tun_name);

	routes = cJSON_lookup_obj(conf, ".RoutePrefix", NULL);
	if (routes && routes->type==cJSON_Array) {
		int i;
		for (i=0; i<cJSON_GetArraySize(routes); ++i) {
			cJSON *entry;
			entry = cJSON_GetArrayItem(routes, i);
			if (entry->type == cJSON_String) {
				shell("ip route add %s dev %s via %s", entry->valuestring, tun_name, tun_peer_addr);
			}
		}
	}

	default_route = cJSON_lookup_str(conf, ".DefaultRoute", NULL);
	if (default_route!=NULL) {
		shell("ip route add default dev %s table %s", tun_name, default_route);
	}

	pthread_create(&tid_tun_reader, NULL, thr_tun_reader, NULL);

	pthread_join(tid_tun_reader, NULL);

	carrier->destroy(carrier_ctx);

	close(tun_fd);

	return 0;
}

