#ifndef JSON_CONF_H
#define JSON_CONF_H

/**
	\file gen_conf.h
		\brief Generic json configure file processing.
*/
/** \cond 0 */
#include <cJSON.h>
/** \endcond */

#define	MAX_LOAD_DEPTH	16

#define LOAD_KEYWORD    "load_json"

extern cJSON *global_config;

cJSON *conf_load_file(const char *filename);
#define	global_conf_load_file(F) do{global_config=conf_load_file(F);}while(0)

/**
	\fn	int conf_delete(cJSON *)
		\brief Destroy the internal global configure struct.
*/
int conf_delete(cJSON *);
#define global_conf_delete() conf_delete(global_config)

#if 0
cJSON *conf_combine(cJSON *to, cJSON *from);
#endif

cJSON *conf_get(const char *, cJSON *, const cJSON *);
#define global_conf_get(N, D) conf_get(N, D, global_config)

int conf_get_int(const char *, int, const cJSON *);
#define	global_conf_get_int(N, D) conf_get_int(N, D, global_config)
int conf_get_bool(const char *, int, const cJSON *);
#define	global_conf_get_bool(N, D) conf_get_bool(N, D, global_config)
const char *conf_get_str(const char *, const char *, const cJSON *);
#define	global_conf_get_str(N, D) conf_get_str(N, D, global_config)

#endif

