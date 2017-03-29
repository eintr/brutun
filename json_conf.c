/** \cond 0 */
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include "cJSON.h"
#include <stdlib.h>
/** \endcond */

#include "json_conf.h"

cJSON *global_config=NULL;

static cJSON *process_json(cJSON *conf, int level);

#define MEMSTEP 1024
cJSON *conf_parse(FILE *fp)
{
    char *str=NULL;
    int strsize=0, freecount=0;
    cJSON *res=NULL;
    char c[2]={0,0};
    const char *openmark="[{", *closemark="]}";
    int level=0, begun=0, initial=1, line_start=1;
    int in_comment=0;
    int ret;

    while ((ret = fread(c, 1, 1, fp))) {
        if (ret < 0 && (errno == EINTR || errno == EAGAIN)) {
            continue;
        }

        if (in_comment) {
            if (c[0]=='\n') {
                in_comment = 0;
                line_start=1;
                continue;
            }
            continue;
        }

        if (line_start) {
            if (c[0]=='#') {
                in_comment = 1;
                line_start=0;
                continue;
            }
            line_start=0;
        }

        if (c[0]=='\n') {
            line_start=1;
            c[0] = ' ';
        }

        if (strchr(openmark, c[0])!=NULL) { // Found an open mark
            begun=1;
            level++;
        } else if (strchr(closemark, c[0])!=NULL) { // Found an open mark
            level--;
        }
        if (begun) {
            if (freecount<1) {
                str = realloc(str, strsize+MEMSTEP+1);
                if (str==NULL) {
                    goto quit;
                }
                if (initial) {
                    str[0]='\0';
                    initial=0;
                }
                strsize += MEMSTEP;
                freecount += MEMSTEP;
            }
            strcat(str, c);
            freecount--;
        }

        if (begun && level==0) {
            break;
        }
    }
    if (begun && level==0) {
        res = cJSON_Parse(str);
    }
quit:
    free(str);
    return res;
}

static cJSON *conf_load_simple(const char *filename)
{
	FILE *fp;
	cJSON *conf;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		//log error
		fprintf(stderr, "Conf load failed in fopen(): %m");
		return NULL;
	} 
	conf = conf_parse(fp);
	fclose(fp);

	return conf;
}

static cJSON *process_array(cJSON *conf, int level)
{
	cJSON *after, *curr;

	for (curr=conf->child; curr!=NULL; curr=curr->next) {
		after = process_json(curr, level);
		if (after!=curr) {
			if (curr->prev==NULL) {
				conf->child = after;
			}
			//cJSON_Delete(curr);
			curr = after;
		}
	}
	return conf;
}

static cJSON *process_json(cJSON *conf, int level)
{
	//cJSON *load, *fname, *result;

	if (conf==NULL) {
		return NULL;
	}

	if (level>=MAX_LOAD_DEPTH) {
		fprintf(stderr, "Followed \""LOAD_KEYWORD"\" more than %d levels, there might be a loop.\n", MAX_LOAD_DEPTH);
		return conf;
	}
	switch (conf->type) {
		case cJSON_Array:
		case cJSON_Object:
			return process_array(conf, level);
			break;
		//case cJSON_Object:
		//	fname = cJSON_GetObjectItem(conf, LOAD_KEYWORD);
		//	if (fname==NULL || fname->type != cJSON_String) {
		//		return process_array(conf, level);
		//	}
		//	load = conf_load_simple(fname->valuestring);
		//	if (load==NULL) {
		//		fprintf(stderr, "Load %s failed.\n", fname->valuestring);
		//		exit(-1);
		//	}
		//	load->next = conf->next;
		//	if (conf->next) {
		//		conf->next->prev = load;
		//	}
		//	load->prev = conf->prev;
		//	if (conf->prev) {
		//		conf->prev->next = load;
		//	}
		//	if (conf->string) {
		//		load->string = strdup(conf->string); // FIXME: Should alloc memory by cJSON_malloc().
		//	} else {
		//		load->string = NULL;
		//	}
		//	result = process_json(load, level+1);

		//	return result;
		//	break;
		case cJSON_String:
		case cJSON_Number:
		case cJSON_NULL:
		case cJSON_True:
		case cJSON_False:
			return conf;
			break;
		default:
			fprintf(stderr, "Unknown type: %d\n", conf->type);
			abort();
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

cJSON *conf_get(const char *name, cJSON *deft, const cJSON *c)
{
	cJSON *tmp;
	if (c==NULL) {
		return deft;
	}
	tmp = cJSON_GetObjectItem((void*)c, name);
	if (tmp) {
		return tmp;
	} else {
		return deft;
	}
}

int conf_get_int(const char *name, int deft, const cJSON *c)
{
	cJSON *tmp;
	if (c==NULL) {
		return deft;
	}
	tmp = cJSON_GetObjectItem((void*)c, name);
	if (tmp && tmp->type == cJSON_Number) {
		return tmp->valueint;
	}
	return deft;
}

int conf_get_bool(const char *name, int deft, const cJSON *c)
{
	cJSON *tmp;
	if (c==NULL) {
		return deft;
	}
	tmp = cJSON_GetObjectItem((void*)c, name);
	if (tmp && tmp->type == cJSON_True) {
		return 1;
	} else if (tmp && tmp->type == cJSON_False) {
		return 0;
	} else {
		return deft;
	}
}

const char *conf_get_str(const char *name, const char *deft, const cJSON *c)
{
	cJSON *tmp;
	if (c==NULL) {
		return deft;
	}
	tmp = cJSON_GetObjectItem((void*)c, name);
	if (tmp) {
		return tmp->valuestring;
	}
	return deft;
}

int conf_delete(cJSON *conf)
{
	cJSON_Delete(conf);
	return 0;
}

#if UTEST
int main(int argc, char **argv) 
{
	cJSON *conf;

	conf = conf_load(argv[1]);
	printf("Conf Loaded:\n");
	cJSON_fPrint(stderr, conf);
	conf_delete(conf);
	return 0;
}
#endif

