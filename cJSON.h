/**
	\file cJSON.h
	\brief A simple but handful C implementation of cJSON by Dave Gamble.
  Copyright (c) 2009 Dave Gamble
 
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
 
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
 
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef cJSON__h
#define cJSON__h

#ifdef __cplusplus
extern "C"
{
#endif

/* cJSON Types: */
/** \def cJSON_False
	Is boolean false.
*/
#define cJSON_False 0

/** \def cJSON_True
	Is boolean true.
*/
#define cJSON_True 1

/** \def cJSON_NULL
	Is NULL value.
*/
#define cJSON_NULL 2

/** \def cJSON_Number
	Is a numberic value.
*/
#define cJSON_Number 3

/** \def cJSON_String
	Is a string value.
*/
#define cJSON_String 4

/** \def cJSON_Array
	Is an array.
*/
#define cJSON_Array 5

/** \def cJSON_Object
	Is a generic object.
*/
#define cJSON_Object 6
	
#define cJSON_IsReference 256

/** The cJSON structure. */
typedef struct cJSON {
	struct cJSON *next,*prev;	/**< next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
	struct cJSON *child;		/**< An array or object item will have a child pointer pointing to a chain of the items in the array/object. */

	int type;					/**< The type of the item, as above. */

	char *valuestring;			/**< The item's string, if type==cJSON_String */
	int valueint;				/**< The item's number, if type==cJSON_Number */
	double valuedouble;			/**< The item's number, if type==cJSON_Number */

	char *string;				/**< The item's name string, if this item is the child of, or is in the list of subitems of an object */
} cJSON;

/** The cJSON hook structure. */
typedef struct cJSON_Hooks {
      void *(*malloc_fn)(size_t sz);	/**< Memory allocating function */
      void (*free_fn)(void *ptr);		/**< Memory freeing function */
} cJSON_Hooks;

/** Hook the cJSON memory operation. Supply malloc, realloc(TODO) and free functions in cJSON_Hooks to cJSON.
	\param hooks Pointer point to cJSON_Hooks.
*/
extern void cJSON_InitHooks(cJSON_Hooks* hooks);


/* Supply a block of JSON, and this returns a cJSON object you can interrogate. Call cJSON_Delete when finished. */
extern cJSON *cJSON_Parse(const char *value);
extern cJSON *cJSON_fParse(FILE *fp);
extern cJSON *cJSON_fdParse(int fd);
/* Render a cJSON entity to text for transfer/storage. Free the char* when finished. */
extern char  *cJSON_Print(cJSON *item);
extern int cJSON_fPrint(FILE *fp, cJSON *item);
extern int cJSON_fdPrint(int fd, cJSON *item);
/* Render a cJSON entity to text for transfer/storage without any formatting. Free the char* when finished. */
extern char  *cJSON_PrintUnformatted(cJSON *item);
/* Delete a cJSON entity and all subentities. */
extern void   cJSON_Delete(cJSON *c);

/* Returns the number of items in an array (or object). */
extern int	  cJSON_GetArraySize(cJSON *array);
/* Retrieve item number "item" from array "array". Returns NULL if unsuccessful. */
extern cJSON *cJSON_GetArrayItem(cJSON *array,int item);
/* Get item "string" from object. Case insensitive. */
extern cJSON *cJSON_GetObjectItem(cJSON *object,const char *string);

/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when cJSON_Parse() returns 0. 0 when cJSON_Parse() succeeds. */
extern const char *cJSON_GetErrorPtr();
	
/* These calls create a cJSON item of the appropriate type. */
/** Create cJSON with value NULL */
extern cJSON *cJSON_CreateNull();
/** Create cJSON with boolean value True */
extern cJSON *cJSON_CreateTrue();
/** Create cJSON with boolean value False */
extern cJSON *cJSON_CreateFalse();
/** Create cJSON boolean value
	\param b Boolean value
*/
extern cJSON *cJSON_CreateBool(int b);
/** Create cJSON with numberic value
	\param num Numberic value
*/
extern cJSON *cJSON_CreateNumber(double num);
/** Create cJSON with string value
	\param string ASCIIZ string value
*/
extern cJSON *cJSON_CreateString(const char *string);
/** Create cJSON array */
extern cJSON *cJSON_CreateArray();
/** Create cJSON generic object */
extern cJSON *cJSON_CreateObject();

/* These utilities create an Array of count items. */
/** Create cJSON numberic array with an int array */
extern cJSON *cJSON_CreateIntArray(int *numbers,int count);
/** Create cJSON numberic array with a float array */
extern cJSON *cJSON_CreateFloatArray(float *numbers,int count);
/** Create cJSON numberic array with a double array */
extern cJSON *cJSON_CreateDoubleArray(double *numbers,int count);
/** Create cJSON numberic array with an ASCIIZ string array */
extern cJSON *cJSON_CreateStringArray(const char **strings,int count);

/** Append item to the specified array */
extern void cJSON_AddItemToArray(cJSON *array, cJSON *item);
/** Append item to the specified object. */
extern void	cJSON_AddItemToObject(cJSON *object,const char *string,cJSON *item);
/* Append reference to item to the specified array/object. Use this when you want to add an existing cJSON to a new cJSON, but don't want to corrupt your existing cJSON. */
extern void cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item);
extern void	cJSON_AddItemReferenceToObject(cJSON *object,const char *string,cJSON *item);

/* Remove/Detatch items from Arrays/Objects. */
extern cJSON *cJSON_DetachItemFromArray(cJSON *array,int which);
extern void   cJSON_DeleteItemFromArray(cJSON *array,int which);
extern cJSON *cJSON_DetachItemFromObject(cJSON *object,const char *string);
extern void   cJSON_DeleteItemFromObject(cJSON *object,const char *string);
	
/* Update array items. */
extern void cJSON_ReplaceItemInArray(cJSON *array,int which,cJSON *newitem);
extern void cJSON_ReplaceItemInObject(cJSON *object,const char *string,cJSON *newitem);

#define cJSON_AddNullToObject(object,name)	cJSON_AddItemToObject(object, name, cJSON_CreateNull())
#define cJSON_AddTrueToObject(object,name)	cJSON_AddItemToObject(object, name, cJSON_CreateTrue())
#define cJSON_AddFalseToObject(object,name)		cJSON_AddItemToObject(object, name, cJSON_CreateFalse())
#define cJSON_AddNumberToObject(object,name,n)	cJSON_AddItemToObject(object, name, cJSON_CreateNumber(n))
#define cJSON_AddStringToObject(object,name,s)	cJSON_AddItemToObject(object, name, cJSON_CreateString(s))

#ifdef __cplusplus
}
#endif

#endif
