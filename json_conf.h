#ifndef JSON_CONF_H
#define JSON_CONF_H

/**
	\file gen_conf.h
		\brief Generic json configure file processing.
*/
/** \cond 0 */
#include "cJSON.h"
/** \endcond */

#define	MAX_LOAD_DEPTH	16

#define LOAD_KEYWORD    "load_json"

cJSON *conf_load_file(const char *filename);

/**
	\fn	int conf_delete(cJSON *)
		\brief Destroy the internal global configure struct.
*/
int conf_delete(cJSON *);

cJSON *conf_get(const char *, cJSON *);

int conf_get_int(const char *, cJSON *);
double conf_get_double(const char *, cJSON *);
int conf_get_bool(const char *, cJSON *);
char *conf_get_str(const char *, cJSON *);

#endif

