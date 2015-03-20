/**
	\file ds_llist.h
	Linked list implementation
*/

#ifndef LLIST_H
#define LLIST_H

/** \cond 0 */
#include <stdint.h>
#include <pthread.h>
#include "cJSON.h"
/** \endcond */


/** Linked-list node structure */
typedef struct llist_node_st {
	struct llist_node_st *prev, *next;
	void *ptr;	/**< Points to user-maintained data block */
} llist_node_t;

/** Linked-list structure */
typedef struct llist_st {
	int nr_nodes;			/**< Number of nodes contained */
	int volume;				/**< Max number of nodes */
	llist_node_t dumb;		/**< Head dumb node */
	pthread_mutex_t lock;	/**< Mutual lock */
	pthread_cond_t cond;	/**< Condition variable */
} llist_t;

/**
 * Create a new empty llist.
	\param volume Max number of nodes
	\return Address of the newly created llist structure. NULL when failed.
 */
llist_t *llist_new(int volume);

/**
 * Destroy a llist.
	\return 0 if OK.
 */
int llist_delete(llist_t*);

/**
 * Travel a llist.
	\param ll The linked-list you want to travel
	\param func The call-back function that processes every llist node
	\return 0 if OK
 */
int llist_travel(llist_t *ll, void (*func)(void *));
int llist_travel_nb(llist_t *ll, void (*func)(void *));

/**
 * Add a node to the end of the llist
 */
int llist_append(llist_t*, void *data);
int llist_append_nb(llist_t*, void *data);

/*
 * Add a node to the head of the llist
 */
int llist_prepend(llist_t*, void *data);
int llist_prepend_nb(llist_t*, void *data);

/**
 * Get the data ptr of the first node.
 */
int llist_get_head(llist_t*, void**);
int llist_get_head_nb(llist_t*, void**);

/**
 * Get the data ptr of the first node and delete the node.
	\return 0 if OK, -1 if failed.
 */
int llist_fetch_head(llist_t*, void**);
int llist_fetch_head_nb(llist_t*, void**);

/**
 * Get the next node of ptr
 */
//void * llist_get_next_unlocked(llist_t *ll, void *ptr);
void * llist_get_next_nb(llist_t *ll, void *ptr);

/**
 * Get the first node
 */
//int llist_get_head_node_unlocked(llist_t *ll, void **node);
int llist_get_head_node_nb(llist_t *ll, void **node);

/**
 * Dump out the info of an llist
 */
cJSON * llist_info_json(llist_t *ll);

#endif

