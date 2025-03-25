#ifndef UTIL_JCONF_H
#define UTIL_JCONF_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "cJSON.h"

cJSON *cJSON_loadfile(const char *fname);

const cJSON *cJSON_lookup_obj(const cJSON*, const char *path, const cJSON *deflt);
int cJSON_lookup_int(const cJSON*, const char *path, int deflt);
double cJSON_lookup_double(const cJSON*, const char *path, double deflt);
const char *cJSON_lookup_str(const cJSON*, const char *path, const char* deflt);
int cJSON_lookup_bool(const cJSON*, const char *path, int deflt);

#ifdef __cplusplus
}
#endif

#endif

