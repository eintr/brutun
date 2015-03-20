/********************************************************
 *  Sorry, no garbage infomations here.
 *  Lisence: read the LISENCE file,
 *  Author(s): read the AUTHOR file and the git commit log,
 *  Code history: read the git commit log.
 *  That's all.
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <linux/if.h>
#include <linux/if_tun.h>

#include "protocol.h"
#include "relayer.h"
#include "json_conf.h"

static char *config_file;

static void parse_args(int argc, char **argv)
{
	int c;

	do {
		c=getopt(argc, argv, "c:");
		switch (c) {
			case 'c':
				config_file = optarg;
				break;
			default:
				break;
		}
	} while (c!=-1);

	if (config_file==NULL) {
		fprintf(stderr, "Usage:!!\n");
		abort();
	}
}

static int tun_alloc(char *dev, int flags) {

	struct ifreq ifr;
	int fd, err;
	char *clonedev = "/dev/net/tun";

	/* Arguments taken by the function:
	 *
	 * char *dev: the name of an interface (or '\0'). MUST have enough
	 *   space to hold the interface name if '\0' is passed
	 * int flags: interface flags (eg, IFF_TUN etc.)
	 */

	/* open the clone device */
	if( (fd = open(clonedev, O_RDWR)) < 0 ) {
		return fd;
	}

	/* preparation of the struct ifr, of type "struct ifreq" */
	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_flags = flags;   /* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

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

static int shell(const char *cmd)
{
	int ret;

	printf("run: %s  ...  ", cmd);
	ret = system(cmd);
	if (ret==-1) {
		printf("failed: %m.\n");
	} else {
		printf("status=%d.\n", ret);
	}
	return ret;
}

#define	BUFSIZE	1024

int
main(int argc, char **argv)
{
	int sd, tun_fd;
	char tun_name[IFNAMSIZ];
	struct sockaddr_in local_addr;
	char cmdline[BUFSIZE];
	cJSON *conf;
	char *tun_local_addr, *tun_peer_addr;

	parse_args(argc, argv);

	conf = conf_load_file(config_file);
	if (conf==NULL) {
		fprintf(stderr, "Load config failed.\n");
		exit(1);
	}

	tun_local_addr = conf_get_str("TunnelLocalAddr", conf);
	tun_peer_addr = conf_get_str("TunnelPeerAddr", conf);
	if (tun_local_addr==NULL || tun_peer_addr==NULL) {
		fprintf(stderr, "Must define TunnelLocalAddr and TunnelPeerAddr in config file!\n");
		exit(1);
	}

	sd = socket(PF_INET, SOCK_DGRAM, 0);
	if (sd<0) {
		perror("socket()");
		exit(1);
	}

	local_addr.sin_family = PF_INET;
	local_addr.sin_addr.s_addr = 0;
	local_addr.sin_port = htons(conf_get_int("LocalPort", conf));
	if (bind(sd, (void*)&local_addr, sizeof(local_addr))<0) {
		perror("bind()");
		exit(1);
	}

	tun_name[0]='\0';
	tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
	if (tun_fd<0) {
		perror("tun_alloc()");
		exit(1);
	}

	snprintf(cmdline, BUFSIZE, "ip addr add dev %s %s peer %s", tun_name, tun_local_addr, tun_peer_addr);
	shell(cmdline);
	snprintf(cmdline, BUFSIZE, "ip link set dev %s up", tun_name);
	shell(cmdline);

	relay(sd, tun_fd, conf);

	close(sd);
	close(tun_fd);

	return 0;
}

