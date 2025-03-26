/********************************************************
 *  Sorry, no garbage infomations here.
 *  Lisence: read the LISENCE file,
 *  Author(s): read the AUTHOR file and the git commit log,
 *  Code history: read the git commit log.
 *  That's all.
 ********************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "util_cjson.h"

#include "carrier.h"
#include "tunnel.h"

static volatile int sig_exit_received = 0;

static void sig_exit(int s)
{
	sig_exit_received = 1;
}

int main(int argc, char **argv)
{
	cJSON *conf;
	struct tunnel_ctx *t;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
		abort();
	}

	signal(SIGTERM, sig_exit);
	signal(SIGINT, sig_exit);
	signal(SIGQUIT, sig_exit);

	srand(getpid());

	conf = cJSON_loadfile(argv[1]);
	if (conf == NULL) {
		fprintf(stderr, "Load config failed.\n");
		exit(1);
	}

	t = tunnel_new(conf);
	assert(t != NULL);

	while (sig_exit_received == 0)
		pause();

	tunnel_delete(t);
	cJSON_Delete(conf);
	return 0;
}
