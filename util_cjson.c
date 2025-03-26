#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <printf.h>
#include <ctype.h>

#include "util_cjson.h"

#define CJSON_INCLUDE_PREFIX "IncludeJSON:"

extern void reg_util_cjson_printf_handler(void) __attribute__((constructor));

static char *include_value(char *s)
{
	if (strncasecmp(s, CJSON_INCLUDE_PREFIX, strlen(CJSON_INCLUDE_PREFIX)) == 0) {
		int l;
		for (s += strlen(CJSON_INCLUDE_PREFIX); isspace(*s); s++) ;
		for (l = strlen(s) - 1; isspace(s[l]) && l >= 0; l--) {
			s[l] = 0;
		}
		return s;
	}
	return NULL;
}

static void process_include(cJSON *j)
{
	if (j->type == cJSON_Object) {
		cJSON *current, *next;
		for (current = j->child; current != NULL; current = next) {
			next = current->next;
			if (current->type == cJSON_String) {
				char *fname = include_value(current->valuestring);
				if (fname != NULL) {
					cJSON *subfile = cJSON_loadfile(fname);
					if (subfile != NULL) {
						cJSON_ReplaceItemInObject(j, current->string, subfile);
					}
				}
			}
		}
	} else if (j->type == cJSON_Array) {
		cJSON *current;
		for (current = j->child; current != NULL; current = current->next) {
			process_include(current);
		}
	}
}

cJSON *cJSON_loadfile(const char *fname)
{
	char buf[65536];
	FILE *f;
	int len;
	cJSON *root;

	f = fopen(fname, "r");
	if (f == NULL) {
		fprintf(stderr, "fopen(%s): %m", fname);
		return NULL;
	}
	len = fread(buf, 1, 65536, f);
	if (len <= 0) {
		perror("fread()");
		fclose(f);
		return NULL;
	}
	buf[len] = 0;
	fclose(f);
	root = cJSON_Parse(buf);
	if (root == NULL) {
		return NULL;
	}
	process_include(root);
	return root;
}

const cJSON *cJSON_lookup_obj(const cJSON *j, const char *path, const cJSON *deflt)
{
	//fprintf(stderr, "Lookup: %s in %s\n", path, cJSON_PrintUnformatted(j));
	if (path[0] == '.') {
		if (path[1] == 0) {
			return j;
		} else {
			int i;
			char *key;
			int keylen = 0;
			cJSON *value;
			key = strdup(path + 1);
			for (i = 0; key[i] != 0; ++i) {
				if (key[i] == '\\') {
					int p = 0;
					do {
						key[p + i] = key[p + i + 1];
						++p;
					} while (key[p + i] != 0);
					keylen++;
				} else if (key[i] == '.' || key[i] == 0 || key[i] == '[') {
					key[i] = 0;
					break;
				}
			}
			keylen += i;
			//fprintf(stderr, "Key seg: %s, len=%d\n", key, i);
			value = cJSON_GetObjectItem(j, key);
			free(key);
			//fprintf(stderr, "Value: %p\n", value);
			if (value == NULL) {
				return deflt;
			}
			if (path[keylen + 1] == 0) {
				return value;
			} else {
				return cJSON_lookup_obj(value, path + keylen + 1, deflt);
			}
		}
	} else if (path[0] == '[') {
		int i, sub = -1;
		char *key;
		if (j->type != cJSON_Array) {
			return deflt;
		}
		key = strdup(path + 1);
		for (i = 0; key[i] != 0; ++i) {
			if (key[i] == ']') {
				key[i] = 0;
				sub = atoi(key);
				break;
			} else if (!isdigit(key[i])) {
				free(key);
				return deflt;
			}
		}
		free(key);
		if (sub == -1) {
			return deflt;
		}
		//fprintf(stderr, "Lookup elm %d in %s\n", sub, cJSON_PrintUnformatted(j));
		if (path[i + 2] == 0) {
			return cJSON_GetArrayItem(j, sub);
		} else {
			return cJSON_lookup_obj(cJSON_GetArrayItem(j, sub), path + i + 2, deflt);
		}
	} else {
		fprintf(stderr, "Invalid lookup key_path: %s\n", path);
		return deflt;
	}
}

int cJSON_lookup_int(const cJSON *self, const char *path, int deflt)
{
	const cJSON *obj;
	obj = cJSON_lookup_obj(self, path, NULL);
	if (obj == NULL) {
		return deflt;
	}
	if (obj->type != cJSON_Number) {
		return deflt;
	}
	return obj->valueint;
}

double cJSON_lookup_double(const cJSON *self, const char *path, double deflt)
{
	const cJSON *obj;
	obj = cJSON_lookup_obj(self, path, NULL);
	if (obj == NULL) {
		return deflt;
	}
	if (obj->type != cJSON_Number) {
		return deflt;
	}
	return obj->valuedouble;
}

const char *cJSON_lookup_str(const cJSON *self, const char *path, const char *deflt)
{
	const cJSON *obj;
	obj = cJSON_lookup_obj(self, path, NULL);
	if (obj == NULL) {
		return deflt;
	}
	if (obj->type != cJSON_String) {
		return deflt;
	}
	return obj->valuestring;
}

int cJSON_lookup_bool(const cJSON *self, const char *path, int deflt)
{
	const cJSON *obj;
	obj = cJSON_lookup_obj(self, path, NULL);
	if (obj == NULL) {
		return deflt;
	}
	if (obj->type == cJSON_False) {
		return 0;
	} else if (obj->type == cJSON_True) {
		return 1;
	}
	return deflt;
}

static int printf_handler(FILE *f, const struct printf_info *info, const void *const *args)
{
	const cJSON *j;
	char *tmp;
	int n;

	j = *((const cJSON **)(args[0]));
	tmp = cJSON_Print(j);
	n = fprintf(f, "%s", tmp);
	free(tmp);

	return n;
}

static int print_ais(const struct printf_info *info, size_t n, int *argtypes, int *size)
{
	/* We always take exactly one argument and this is a pointer to the
	 *         structure.. */
	if (n > 0) {
		argtypes[0] = PA_POINTER;
		size[0] = sizeof(void *);
	}
	return 1;
}

void reg_util_cjson_printf_handler(void)
{
#ifdef UT
	fprintf(stderr, "reg_util_cjson_printf_handler()\n");
#endif
	register_printf_specifier('J', printf_handler, print_ais);
}

#ifdef UT
#include <assert.h>
#define PASS_WHEN(x) if (x) {printf("PASSED %s\n", __func__);return 1;} else {printf("FAILED %s\n", __func__);return 0;}
cJSON *c;

static int test_loadfile(void)
{
	c = cJSON_loadfile("ut.json");
	PASS_WHEN(c != NULL);
}

static int test_printf(cJSON *c)
{
	char *expected_result = "{\"key_str\":\"value_string\",\"key_int\":42,\"key_double\":3.1415927,\"key_true\":true,\"key_false\":false,\"key_null\":null,\"Array1\":[0,1,2,3,4,5,6,7,8],\"Map01\":{\"key_str\":\"value_string\",\"key_int\":42,\"key_double\":3.1415927,\"key_true\":true,\"key_false\":false,\"key_null\":null},\"IncludeFile\":{\"ikey\":\"ivalue\"},\"Array2\":[{\"Array\":[\"str0\",\"str1\",\"str2\",\"str3\"]},{\"A.r.r.a.y\":[0,1,2,3]}],\"Array3\":[[\"str00\",\"str01\",\"str02\",\"str03\"],[\"str10\",\"str11\",\"str12\",\"str13\"],[\"str20\",\"str21\",\"str22\",\"str23\"],[\"str30\",\"str31\",\"str32\",\"str33\"]],\"Steps.Per.Round\":20000,\"Jbus\":{\"Enabled\":true}}";
	char result[1024];
	snprintf(result, 1023, "%J", c);
	result[1023] = 0;
	PASS_WHEN(strcmp(result, expected_result) == 0);
}

static int test_loadfile_include(cJSON *c)
{
	PASS_WHEN(strcmp(cJSON_lookup_str(c, ".IncludeFile.ikey", ""), "ivalue") == 0);
}

static int test_simple_key_map_get_obj(cJSON *jc)
{
	const cJSON *result;

	result = cJSON_lookup_obj(jc, ".Map01", NULL);
	PASS_WHEN(result == cJSON_GetObjectItem(jc, "Map01"));
}

static int test_simple_key_map_get_int(cJSON *c)
{
	PASS_WHEN(cJSON_lookup_int(c, ".key_int", -1) == 42);
}

static int test_simple_key_map_get_str(cJSON *c)
{
	PASS_WHEN(strcmp(cJSON_lookup_str(c, ".key_str", ""), "value_string") == 0);
}

static int test_simple_key_map_get_true(cJSON *c)
{
	PASS_WHEN(cJSON_lookup_bool(c, ".key_true", 0));
}

static int test_simple_key_map_get_false(cJSON *c)
{
	PASS_WHEN(!cJSON_lookup_bool(c, ".key_false", 1));
}

static int test_simple_key_map_get_false2(cJSON *c)
{
	PASS_WHEN(cJSON_lookup_bool(c, ".Jbus.Enabled", 0));
}

static int test_simple_key_map_get_null(cJSON *c)
{
}

static int test_simple_array_get_elm(cJSON *c)
{
	PASS_WHEN(cJSON_lookup_int(c, ".Array1[1]", -1) == 1 && cJSON_lookup_int(c, ".Array1[4]", -1) == 4);
}

static int test_simple_key_escape(cJSON *c)
{
	PASS_WHEN(cJSON_lookup_int(c, ".Steps\\.Per\\.Round", -1) == 20000);
}

static int test_simple_complex_key(cJSON *c)
{
	PASS_WHEN(cJSON_lookup_int(c, ".Array2[1].A\\.r\\.r\\.a\\.y[3]", -1) == 3);
}

static int test_torture_map_key_404(cJSON *c)
{
	PASS_WHEN(cJSON_lookup_int(c, ".no_such_key[1].no_such_key_either", -1)
		  == -1);
}

static int test_torture_array_currupt_sub(cJSON *c)
{
	PASS_WHEN(cJSON_lookup_int(c, ".Array1[3", -1) == -1);
}

static int test_torture_array_nondigit_sub(cJSON *c)
{
	PASS_WHEN(cJSON_lookup_int(c, ".Array1[X]", -1) == -1);
}

static int test_torture_array_overflow_sub(cJSON *c)
{
	PASS_WHEN(cJSON_lookup_int(c, ".Array1[9]", -1) == -1);
}

static int test_torture_loadfile_invalid(void)
{
	cJSON *o;
	o = cJSON_loadfile(__FILE__);
	if (o != NULL) {
		cJSON_Delete(o);
	}
	PASS_WHEN(o == NULL);
}

int main()
{
	test_loadfile();

	test_simple_key_map_get_obj(c);
	test_simple_key_map_get_int(c);
	test_simple_key_map_get_str(c);
	test_simple_key_map_get_true(c);
	test_simple_key_map_get_false(c);
	test_simple_key_map_get_false2(c);
	test_loadfile_include(c);
	test_printf(c);

	test_simple_array_get_elm(c);

	test_simple_key_escape(c);
	test_simple_complex_key(c);

	test_torture_loadfile_invalid();
	test_torture_map_key_404(c);
	test_torture_array_currupt_sub(c);
	test_torture_array_nondigit_sub(c);
	test_torture_array_overflow_sub(c);

	cJSON_Delete(c);
}
#endif
