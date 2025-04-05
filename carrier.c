#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>

#include "util_cjson.h"

#include "carrier.h"

struct carrier *carrier_load(const cJSON *conf, void (*on_recv)(void *tun, void *, size_t), void *tun)
{
	struct carrier *ret;
	if (conf == NULL) {
		return NULL;
	}
	ret = malloc(sizeof(*ret));
	assert(ret != NULL);
	{
		const char *carrier_mod;
		char *cwd, *carrier_mod_path;

		carrier_mod = cJSON_lookup_str(conf, ".Module", NULL);
		assert(carrier_mod != NULL);
		fprintf(stderr, "Loading: %s\n", carrier_mod);

		carrier_mod_path = malloc(2 + strlen(carrier_mod) + 1);
		assert(carrier_mod_path != NULL);
		sprintf(carrier_mod_path, "./%s", carrier_mod);

		cwd = get_current_dir_name();
		assert(chdir(PLUGINDIR) == 0);
		ret->handler = dlopen(carrier_mod_path, RTLD_NOW);
		if (ret->handler == NULL) {
			fprintf(stderr, "Open plugin %s failed: %s\n", carrier_mod_path, dlerror());
			abort();
		}
		ret->interface = dlsym(ret->handler, "carrier_interface");
		assert(ret->interface != NULL);
		assert(chdir(cwd) == 0);
		free(cwd);
		free(carrier_mod_path);
	}

	ret->context = ret->interface->init(cJSON_lookup_obj(conf, ".Config", NULL), tun);
	assert(ret->context != NULL);
	ret->interface->on_packet_receive(ret->context, on_recv);
	fprintf(stderr, "Inited plugin: %s\n", ret->interface->name);
	return ret;
}

void carrier_unload(struct carrier *c)
{
	c->interface->destroy(c->context);
	dlclose(c->handler);
	free(c);
}
