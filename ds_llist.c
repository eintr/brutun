/**	\file ds_llist.c */

/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
/** \endcond */
#include "ds_llist.h"

#include "cJSON.h"
/*
 * return value:
 * 	0 success
 * 	<0 -errno
 */

/**
 * Create a new empty llist
 */
llist_t *llist_new(int max)
{
	llist_t *ll;

	ll = (llist_t *) malloc(sizeof(llist_t));
	if (ll == NULL) {
		return NULL;
	}

	ll->nr_nodes = 0;
	ll->volume = max;
	ll->dumb.prev = ll->dumb.next = &ll->dumb;
	ll->dumb.ptr = NULL;

	pthread_mutexattr_t ma;
	pthread_mutexattr_init(&ma);
	//pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);

	if (pthread_mutex_init(&ll->lock, &ma) != 0) {
		free(ll);
		return NULL;
	}
	if (pthread_cond_init(&ll->cond, NULL) != 0) {
		pthread_mutex_destroy(&ll->lock);
		free(ll);
		return NULL;
	}

	pthread_mutexattr_destroy(&ma);
	return ll;
}

llist_node_t * llist_new_node()
{
	llist_node_t *lnode;
	lnode = (llist_node_t *) malloc(sizeof(llist_node_t));
	if (lnode == NULL) {
		return NULL;
	}
	lnode->ptr = NULL;
	lnode->prev = lnode->next = NULL;
	return lnode;
}


/*
 * Destroy an llist
 */
int llist_delete(llist_t *ll)
{
	void *buf;

	if (ll == NULL) {
		return 0;
	}

	while (ll->nr_nodes > 0) {
		llist_fetch_head_nb(ll, &buf);
	}

	pthread_mutex_destroy(&ll->lock);
	pthread_cond_destroy(&ll->cond);

	free(ll);
	return 0;
}

/*
 * Add a node to the end of the llist
 */
int llist_append_unlocked(llist_t *ll, void *data)
{
	llist_node_t *lnode;
	if (ll->nr_nodes < ll->volume) {
		lnode = llist_new_node();
		if (lnode == NULL) {
			return -1;
		}
		lnode->ptr = data;

		lnode->prev = ll->dumb.prev;
		lnode->next = &ll->dumb;
		ll->dumb.prev->next = lnode;
		ll->dumb.prev = lnode;
		ll->nr_nodes++;

		return 0;
	}
	return -1;
}

int llist_append(llist_t *ll, void *data)
{
	int ret;

	pthread_mutex_lock(&ll->lock);
	while (ll->nr_nodes >= ll->volume) {
		pthread_cond_wait(&ll->cond, &ll->lock);
	}

	ret = llist_append_unlocked(ll, data);

	pthread_cond_broadcast(&ll->cond);
	pthread_mutex_unlock(&ll->lock);
	return ret;
}

int llist_append_nb(llist_t *ll, void *data)
{
	int ret;

	if (pthread_mutex_lock(&ll->lock) != 0)  {
		return -2;
	}

	ret = llist_append_unlocked(ll, data);

	pthread_cond_broadcast(&ll->cond);
	pthread_mutex_unlock(&ll->lock);
	return ret;
}

void * llist_get_next_unlocked(llist_t *ll, void *ptr)
{
	llist_node_t *ln = (llist_node_t *)	ptr;
	if (ln->next == &ll->dumb) {
		return NULL;
	}
	return ln->next;

}

void * llist_get_next_nb(llist_t *ll, void *ptr)
{
	void * next_ptr;

	if (pthread_mutex_lock(&ll->lock) != 0)  {
		return NULL;
	}

	next_ptr = llist_get_next_unlocked(ll, ptr);

	pthread_mutex_unlock(&ll->lock);
	return next_ptr;

}

/*
 * Get the data ptr of the first node.
 */
int llist_get_head_unlocked(llist_t *ll, void **data) 
{
	llist_node_t *ln;
	if (ll == NULL) {
		return -1;
	}
	if (ll->nr_nodes <= 0) {
		return -1;
	}
	ln = ll->dumb.next;
	if (ln != &ll->dumb) {
		*data = ln->ptr;
		return 0;
	}
	return -1;
}

int llist_get_head(llist_t *ll, void **data)
{
	int ret;

	pthread_mutex_lock(&ll->lock);
	while (ll->nr_nodes <= 0) {
		pthread_cond_wait(&ll->cond, &ll->lock);
	}

	ret = llist_get_head_unlocked(ll, data);

	pthread_cond_broadcast(&ll->cond);
	pthread_mutex_unlock(&ll->lock);
	return ret;
}

int llist_get_head_nb(llist_t *ll, void **data)
{
	int ret;

	if (pthread_mutex_lock(&ll->lock) != 0) {
		return -2;
	}

	ret = llist_get_head_unlocked(ll, data);

	pthread_mutex_unlock(&ll->lock);
	return ret;
}

int llist_get_head_node_unlocked(llist_t *ll, void **node)
{
	llist_node_t *ln;
	if (ll == NULL) {
		return -1;
	}
	if (ll->nr_nodes <= 0) {
		return -1;
	}
	ln = ll->dumb.next;
	if (ln != &ll->dumb) {
		*node = ln;
		return 0;
	}
	return -1;
}

int llist_get_head_node(llist_t *ll, void **node)
{
	int ret;

	pthread_mutex_lock(&ll->lock);
	
	ret = llist_get_head_node_unlocked(ll, node);

	pthread_mutex_unlock(&ll->lock);
	return ret;
}

int llist_get_head_node_nb(llist_t *ll, void **node)
{
	int ret;

	if (pthread_mutex_lock(&ll->lock) != 0) {
		return -2;
	}

	ret = llist_get_head_node_unlocked(ll, node);

	pthread_mutex_unlock(&ll->lock);
	return ret;
}

/*
 * Get the data ptr of the first node and delete the node.
 */
int llist_fetch_head_unlocked(llist_t *ll, void **data)
{
	llist_node_t *ln;

	if (ll == NULL) {
		return -1;
	}
	if (ll->nr_nodes <= 0) {
		return -1;
	}
	ln = ll->dumb.next;
	if (ln != &ll->dumb) {
		*data = ln->ptr;
		ll->dumb.next = ln->next;
		ln->next->prev = &ll->dumb;
		ln->prev = ln->next = NULL;
		free(ln);
		ll->nr_nodes--;
		return 0;
	}
	return -1;
}

int llist_fetch_head(llist_t *ll, void **data)
{
	int ret;

	pthread_mutex_lock(&ll->lock);
	while (ll->nr_nodes <= 0) {
		pthread_cond_wait(&ll->cond, &ll->lock);
	}

	ret = llist_fetch_head_unlocked(ll, data);

	pthread_cond_broadcast(&ll->cond);
	pthread_mutex_unlock(&ll->lock);
	return ret;
}

int llist_fetch_head_nb(llist_t *ll, void **data)
{
	int ret;

	if (pthread_mutex_lock(&ll->lock) != 0) {
		return -2;
	}

	ret = llist_fetch_head_unlocked(ll, data);

	pthread_mutex_unlock(&ll->lock);

	return ret;
}

int llist_travel(llist_t *ll, void (*func)(void *data))
{
	llist_node_t *ln;
	if (func == NULL)
		return -1;

	for (ln = ll->dumb.next; ln != &ll->dumb; ln = ln->next) {
		if (ln->ptr) {
			func(ln->ptr);
		}
	}
	return 0;
}


/*
 * Dump out the info of an llist
 */
cJSON *llist_info_json(llist_t* ll)
{
	cJSON *result;

	result = cJSON_CreateObject();
	cJSON_AddNumberToObject(result, "nr_nodes", ll->nr_nodes);
	cJSON_AddNumberToObject(result, "volume", ll->volume);

	return result;
}

#ifdef AA_LLIST_TEST
#include <string.h>
#include <sched.h>
#include "aa_test.h"

void show(void *d)
{
	printf("%c", *(char *)d);
}


void * producer(void *args)
{
	int l=0;
	int num = 0;
	llist_t *ll  = (llist_t *)args;
	char *s = "c";
	while(l++<10000) {
		if (llist_append_nb(ll, &s[0]) == 0)
			num++;
	}
	printf("append success %d\n", num);
}
void * consumer(void *args)
{
	int l=0;
	llist_t *ll = (llist_t *)args;
	void *buf;
	int num =0;
	while(l++<10000) {
		if(llist_fetch_head_nb(ll, &buf) == 0) {
			num++;
		}
		if (l%100 == 0) {
			sched_yield();
		}
	}
	printf("fetch success %d\n", num);
}

int test_llist_match(void *key, void *data)
{
	return strncmp((char *)key, (char *)data, 1);
}

void func_test()
{
	char *s = "this is a test";
	char *s2 = "x";
	void *b;
	int i, j;
	llist_t *ll;

	/*
	 * test 1
	 */

	ll = llist_new(20);	
	for (i = 0; i < 4; i++) {
		if (i%3 == 0) {
			llist_append(ll, &s2[0]);
		}
		llist_append(ll, &s[i]);
	}
	
	llist_travel(ll, &show);
	printf("\n");
	llist_travel(ll, &show);
	printf("\n");
	llist_fetch_head(ll, &b);
	llist_travel(ll, &show);
	printf("\n");
	llist_fetch_head(ll, &b);
	llist_travel(ll, &show);
	printf("\n");
	llist_delete(ll);
	
	printf("%stest1 successful%s\n", COR_BEGIN, COR_END);

	/*
	 * test 2
	 */

	ll = llist_new(1000);
	for (j = 0; j < 100; j++) {
		for (i = 0; i < 10; i++) {
			llist_append_nb(ll, &s[i]);
		}
	}
	printf("append done.\n");
	for (i = 0; i< 100; i++) {
		llist_fetch_head_nb(ll, &b);
		printf("%c", *(char *)b);
	}
	printf("\nthere are %d nodes\n", ll->nr_nodes);
	for (i = 0; i< 100; i++) {
		llist_get_head_nb(ll, &b);
		printf("%c", *(char *)b);
	}
	printf("\n");
	printf("%stest2 successful%s\n", COR_BEGIN, COR_END);
	printf("there are %d nodes\n", ll->nr_nodes);
	
	/*
	 * test 3
	 */
	void *ptr, *ptr2;
	llist_get_head_node_nb(ll, &ptr);
	for (i = 0; i< 100; i++) {
		printf("%c", *(char *)(((llist_node_t *)ptr)->ptr));
		ptr = llist_get_next_nb(ll, ptr);
	}
	printf("\nthere are %d nodes\n", ll->nr_nodes);
	llist_delete(ll);
	printf("%stest3 successful%s\n", COR_BEGIN, COR_END);
	return;
}

void multi_thread_test()
{
	llist_t *ll;
	ll = llist_new(2000);	
	pthread_t pid[10];

	/*
	 * test 4
	 */

	int i;
	for (i=0; i < 6; i++) {
		pthread_create(&pid[i], NULL, producer, ll);
	}
	for (i=6; i<10; i++) {
		pthread_create(&pid[i], NULL, consumer, ll);
	}
	for (i=0; i<10; i++) {
		pthread_join(pid[i], NULL);
	}
	printf("nodes is %d\n", ll->nr_nodes);
	llist_delete(ll);
	printf("%stest4 successful%s\n", COR_BEGIN, COR_END);
}

int main()
{
	func_test();
	multi_thread_test();
	return 0;
}
#endif

