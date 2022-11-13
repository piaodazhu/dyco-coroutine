#ifndef DYCO_HTABLE_H
#define DYCO_HTABLE_H

#include <stdlib.h>

#ifdef __cplusplus 
extern "C"{
#endif

#define DYCO_HTABLE_MAXWITDH		24
#define DYCO_HTABLE_DEFAULTWITDH	16
#define DYCO_HTABLE_MAXALPHA		2

typedef struct dyco_hentry dyco_hentry;
typedef struct dyco_htable dyco_htable;

struct dyco_hentry
{
	int id;
	void *data;
	dyco_hentry *next;
};

struct dyco_htable
{
	int width;
	int mask;
	int count;
	dyco_hentry *table;
};

#define HTABLE_EMPTY(ht)	((ht)->count == 0)
#define HTABLE_SIZE(ht)	((ht)->count)
static dyco_htable* htable_create(int width);
static int htable_init(dyco_htable *ht, int width);
static void htable_clear(dyco_htable *ht);
static void htable_clear_with_freecb(dyco_htable *ht, void (*freecb)(void*));
static int htable_insert(dyco_htable *htable, int id, void *data);
static int htable_delete(dyco_htable *htable, int id, void **data);
static void* htable_find(dyco_htable *htable, int id);
static int htable_contains(dyco_htable *htable, int id);
static int htable_resize(dyco_htable *htable, int width);
static void htable_free(dyco_htable *htable);

static dyco_htable*
htable_create(int width)
{
	dyco_htable *ht = (dyco_htable*)malloc(sizeof(dyco_htable));
	if (ht == NULL)
		return NULL;
	htable_init(ht, width);
	return ht;
}

static int
htable_init(dyco_htable *ht, int width)
{
	if (width <= 0)
		ht->width = DYCO_HTABLE_DEFAULTWITDH;
	else {
		ht->width = width > DYCO_HTABLE_MAXWITDH ? DYCO_HTABLE_MAXWITDH : width;
	}

	int sz = (1 << ht->width);
	ht->mask = sz - 1;
	ht->count = 0;
	ht->table = (dyco_hentry*)calloc(sz, sizeof(dyco_hentry));
	if (ht->table == NULL) {
		return -1;
	}
	int i;
	for (i = 0; i < sz; i++) {
		ht->table[i].id = -1;
		ht->table[i].data = NULL;
		ht->table[i].next = NULL;
	}
	return 0;
}

static void
htable_clear(dyco_htable *ht)
{
	if (ht->table == NULL) 
		return;

	int i;
	dyco_hentry *pre, *ptr;
	for (i = 0; i <= ht->mask; i++) {
		if (ht->table[i].id == -1) {
			continue;
		}
		pre = ht->table[i].next;
		while (pre != NULL) {
			ptr = pre->next;
			free(pre);
			pre = ptr;
		}
	}
	free(ht->table);
	ht->count = 0;
	ht->mask = 0;
	ht->width = 0;
	return;
}

static void
htable_clear_with_freecb(dyco_htable *ht, void (*freecb)(void*))
{
	if (ht->table == NULL) 
		return;

	int i;
	dyco_hentry *pre, *ptr;
	for (i = 0; i <= ht->mask; i++) {
		if (ht->table[i].id == -1) {
			continue;
		}
		freecb(ht->table[i].data);
		pre = ht->table[i].next;
		while (pre != NULL) {
			ptr = pre->next;
			freecb(pre->data);
			free(pre);
			pre = ptr;
		}
	}
	free(ht->table);
	ht->count = 0;
	ht->mask = 0;
	ht->width = 0;
	return;
}

static int
htable_insert(dyco_htable *htable, int id, void *data)
{
	int idx = id & htable->mask;
	dyco_hentry *bucket = &htable->table[idx];
	if (bucket->id == -1) {
		bucket->id = id;
		bucket->data = data;
		++htable->count;
		if (htable->mask * DYCO_HTABLE_MAXALPHA == htable->count) {
			if (htable_resize(htable, htable->width + 1) < 0) {
				return -1;
			}
		}
		return 1;
	}
	dyco_hentry *ptr = bucket;
	while (ptr != NULL) {
		if (ptr->id == id) {
			ptr->data = data;
			return 0;
		}
		ptr = ptr->next;
	}
	dyco_hentry *newnode = (dyco_hentry*)malloc(sizeof(dyco_hentry));
	if (newnode == NULL) {
		return -1;
	}
	newnode->id = id;
	newnode->data = data;
	newnode->next = bucket->next;
	bucket->next = newnode;
	++htable->count;
	if (htable->mask * DYCO_HTABLE_MAXALPHA == htable->count) {
		if (htable_resize(htable, htable->width + 1) < 0) {
			return -1;
		}
	}
	return 1;
}

static int
htable_delete(dyco_htable *htable, int id, void **data)
{
	int idx = id & htable->mask;
	dyco_hentry *bucket = &htable->table[idx];
	if (bucket->id == -1) {
		return -1;
	}
	dyco_hentry *ptr = bucket->next;
	if (bucket->id == id) {
		if (data != NULL)
			*data = bucket->data;
		if (ptr != NULL) {
			bucket->id = ptr->id;
			bucket->data = ptr->data;
			bucket->next = ptr->next;
			free(ptr);
		} else {
			bucket->id = -1;
			bucket->data = NULL;
		}
		--htable->count;
		return 0;
	}
	dyco_hentry *pre = bucket;
	while (ptr != NULL) {
		if (ptr->id == id) {
			if (data != NULL)
				*data = ptr->data;
			pre->next = ptr->next;
			free(ptr);
			--htable->count;
			return 0;
		}
		pre = ptr;
		ptr = ptr->next;
	}
	return -1;
}

static int
htable_delete_with_freecb(dyco_htable *htable, int id, void (*freecb)(void*))
{
	int idx = id & htable->mask;
	dyco_hentry *bucket = &htable->table[idx];
	if (bucket->id == -1) {
		return -1;
	}
	dyco_hentry *ptr = bucket->next;
	if (bucket->id == id) {
		if (bucket->data != NULL)
			freecb(bucket->data);
		if (ptr != NULL) {
			bucket->id = ptr->id;
			bucket->data = ptr->data;
			bucket->next = ptr->next;
			free(ptr);
		} else {
			bucket->id = -1;
			bucket->data = NULL;
		}
		--htable->count;
		return 0;
	}
	dyco_hentry *pre = bucket;
	while (ptr != NULL) {
		if (ptr->id == id) {
			pre->next = ptr->next;
			if (ptr->data != NULL)
				freecb(ptr->data);
			free(ptr);
			--htable->count;
			return 0;
		}
		pre = ptr;
		ptr = ptr->next;
	}
	return -1;
}

static void*
htable_find(dyco_htable *htable, int id)
{
	int idx = id & htable->mask;
	dyco_hentry *bucket = &htable->table[idx];
	if (bucket->id == -1)
		return NULL;
	if (bucket->id == id)
		return bucket->data;

	dyco_hentry *ptr = bucket->next;
	while (ptr != NULL) {
		if (ptr->id == id) {
			return ptr->data;
		}
		ptr = ptr->next;
	}
	return NULL;
}

static int
htable_contains(dyco_htable *htable, int id)
{
	int idx = id & htable->mask;
	dyco_hentry *bucket = &htable->table[idx];
	if (bucket->id == -1)
		return 0;
	if (bucket->id == id)
		return 1;

	dyco_hentry *ptr = bucket->next;
	while (ptr != NULL) {
		if (ptr->id == id) {
			return 1;
		}
		ptr = ptr->next;
	}
	return 0;
}

static int
htable_resize(dyco_htable *htable, int width)
{
	if ((width == htable->width) || (width > DYCO_HTABLE_MAXWITDH))
		return -1;
	int newsize = (1 << width);
	int newmask = newsize - 1;
	dyco_hentry* newtable = (dyco_hentry*)calloc(newsize, sizeof(dyco_hentry));
	if (newtable == NULL) {
		return -1;
	}
	
	int i;
	for (i = 0; i < newsize; i++) {
		newtable->id = -1;
		newtable->data = NULL;
		newtable->next = NULL;
	}

	int oldmask = htable->mask;
	dyco_hentry* oldtable = htable->table;
	dyco_hentry *newnode, *next, *ptr;
	int newidx;
	for (i = 0; i <= oldmask; i++) {
		if (oldtable[i].id == -1) {
			continue;
		}
		// first node
		newidx = oldtable[i].id & newmask;
		if (newtable[newidx].id == -1) {
			newtable[newidx].id = oldtable[i].id;
			newtable[newidx].data = oldtable[i].data;
		} else {
			newnode = (dyco_hentry*)malloc(sizeof(dyco_hentry));
			if (newnode == NULL)
				return -1;
			newnode->id = oldtable[i].id;
			newnode->data = oldtable[i].data;
			newnode->next = newtable[newidx].next;
			newtable[newidx].next = newnode;
		}
		// other node
		ptr = oldtable[i].next;
		while (ptr != NULL) {
			next = ptr->next;
			newidx = ptr->id & newmask;
			if (newtable[newidx].id == -1) {
				newtable[newidx].id = ptr->id;
				newtable[newidx].data = ptr->data;
				free(ptr);
			} else {
				ptr->next = newtable[newidx].next;
				newtable[newidx].next = ptr;
			}
			ptr = next;
		}
	}
	free(oldtable);
	htable->mask = newmask;
	htable->width = width;
	htable->table = newtable;
	return 0;
}

static void
htable_free(dyco_htable *htable)
{
	if (htable == NULL)
		return;

	htable_clear(htable);
	free(htable);
	return;
}

#ifdef __cplusplus 
	}
#endif

#endif