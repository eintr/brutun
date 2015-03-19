/** \cond 0 */
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include "cJSON.h"
/** \endcond */

#include "json_conf.h"

static cJSON *process_json(cJSON *conf, int level);

static cJSON *conf_load_simple(const char *filename)
{
	FILE *fp;
	cJSON *conf;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		//log error
		fprintf(stderr, "fopen(): %m");
		return NULL;
	} 
	conf = cJSON_fParse(fp);
	fclose(fp);

	return conf;
}

static cJSON *process_array(cJSON *conf, int level)
{
	cJSON *after, *curr;
	cJSON *prev, *next;

	for (curr=conf->child; curr!=NULL; curr=curr->next) {
		prev = curr->prev;
		next = curr->next;

		after = process_json(curr, level+1);
		if (after!=curr) {
			fprintf(stderr, "LOAD happened.\n");
			after->next = next;
			after->prev = prev;

			if (prev) {
				prev->next = after;
			} else {
				conf->child = after;
			}
			if (next) {
				next->prev = after;
			}
		//	cJSON_Delete(curr);
			curr = after;
		}
	}
	return conf;
}

static cJSON *process_json(cJSON *conf, int level)
{
	cJSON *load;

	if (conf==NULL) {
		return NULL;
	}
	if (level>=MAX_LOAD_DEPTH) {
		fprintf(stderr, "Followed \""LOAD_KEYWORD"\" too many levels, there might is a loop.\n");
		return conf;
	}
	switch (conf->type) {
		case cJSON_Array:
			return process_array(conf, level);
			break;
		case cJSON_Object:
			load = cJSON_GetObjectItem(conf, LOAD_KEYWORD);
			if (load==NULL) {
				return process_array(conf, level);
			}
			if (load->type == cJSON_String) {
				return process_json(conf_load_simple(load->valuestring), level+1);
			} else {
				return conf;
			}
			break;
		case cJSON_String:
		case cJSON_Number:
		case cJSON_NULL:
		case cJSON_True:
		case cJSON_False:
			return conf;
			break;
		default:
			return NULL;
			break;
	}
}

cJSON *conf_load_file(const char *filename)
{
	cJSON *conf ;

	conf = conf_load_simple(filename);
	if (conf==NULL) {
		return NULL;
	}

	return process_json(conf, 1);
}

cJSON *conf_get(const char *name, cJSON *c)
{
	return cJSON_GetObjectItem(c, name);
}

int conf_get_int(const char *name, cJSON *c)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(c, name);
	if (tmp) {
		return tmp->valueint;
	}
	return -1;
}

double conf_get_double(const char *name, cJSON *c)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(c, name);
	if (tmp) {
		return tmp->valuedouble;
	}
	return -1;
}

int conf_get_bool(const char *name, cJSON *c)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(c, name);
	if (tmp && tmp->type == cJSON_True) {
		return 1;
	}
	return 0;
}
char *conf_get_str(const char *name, cJSON *c)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(c, name);
	if (tmp) {
		return tmp->valuestring;
	}
	return NULL;
}

int conf_delete(cJSON *conf)
{
	cJSON_Delete(conf);
	return 0;
}

#if 0
int main() 
{
	cJSON *conf;

	conf = conf_load("/tmp/main.tmpl");
	printf("Conf Loaded:\n");
	cJSON_fPrint(stdout, conf);
	conf_delete(conf);
	return 0;
}
#endif

