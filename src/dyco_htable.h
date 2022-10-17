#ifndef DYCO_HTABLE_H
#define DYCO_HTABLE_H

#include <stdlib.h>

#define DYCO_HTABLE_MAXWITDH		16
#define DYCO_HTABLE_DEFAULTWITDH	8
#define DYCO_HTABLE_MAXALPHA		2

typedef struct _dyco_hentry dyco_hentry;
typedef struct _dyco_htable dyco_htable;

struct _dyco_hentry
{
	int id;
	void *data;
	dyco_hentry *next;
};

struct _dyco_htable
{
	int _width;
	int _mask;
	int _count;
	dyco_hentry *_table;
};

#define HTABLE_EMPTY(ht)	((ht)->_count == 0)
#define HTABLE_SIZE(ht)	((ht)->_count)
static dyco_htable* _htable_create(int width);
static int _htable_init(dyco_htable *ht, int width);
static void _htable_clear(dyco_htable *ht);
static void _htable_clear_with_freecb(dyco_htable *ht, void (*freecb)(void*));
static int _htable_insert(dyco_htable *htable, int id, void *data);
static int _htable_delete(dyco_htable *htable, int id, void **data);
static void* _htable_find(dyco_htable *htable, int id);
static int _htable_contains(dyco_htable *htable, int id);
static int _htable_resize(dyco_htable *htable, int width);
static void _htable_free(dyco_htable *htable);

static dyco_htable*
_htable_create(int width)
{
	dyco_htable *ht = (dyco_htable*)malloc(sizeof(dyco_htable));
	_htable_init(ht, width);
	return ht;
}

static int
_htable_init(dyco_htable *ht, int width)
{
	// _htable_clear(ht);
	if (width <= 0)
		ht->_width = DYCO_HTABLE_DEFAULTWITDH;
	else {
		ht->_width = width > DYCO_HTABLE_MAXWITDH ? DYCO_HTABLE_MAXWITDH : width;
	}

	int sz = (1 << ht->_width);
	ht->_mask = sz - 1;
	ht->_count = 0;
	ht->_table = (dyco_hentry*)calloc(sz, sizeof(dyco_hentry));
	int i;
	for (i = 0; i < sz; i++) {
		ht->_table[i].id = -1;
		ht->_table[i].data = NULL;
		ht->_table[i].next = NULL;
	}
	return 0;
}

static void
_htable_clear(dyco_htable *ht)
{
	if (ht->_table == NULL) 
		return;

	int i;
	dyco_hentry *pre, *ptr;
	for (i = 0; i <= ht->_mask; i++) {
		if (ht->_table[i].id == -1) {
			continue;
		}
		pre = ht->_table[i].next;
		while (pre != NULL) {
			ptr = pre->next;
			free(pre);
			pre = ptr;
		}
	}
	free(ht->_table);
	ht->_count = 0;
	ht->_mask = 0;
	ht->_width = 0;
	return;
}

static void
_htable_clear_with_freecb(dyco_htable *ht, void (*freecb)(void*))
{
	if (ht->_table == NULL) 
		return;

	int i;
	dyco_hentry *pre, *ptr;
	for (i = 0; i <= ht->_mask; i++) {
		if (ht->_table[i].id == -1) {
			continue;
		}
		printf("tag20\n");
		freecb(ht->_table[i].data);
		pre = ht->_table[i].next;
		printf("tag21\n");
		while (pre != NULL) {
			ptr = pre->next;
			freecb(pre->data);
			free(pre);
			pre = ptr;
		}
	}
	free(ht->_table);
	ht->_count = 0;
	ht->_mask = 0;
	ht->_width = 0;
	return;
}

static int
_htable_insert(dyco_htable *htable, int id, void *data)
{
	int idx = id & htable->_mask;
	dyco_hentry *bucket = &htable->_table[idx];
	if (bucket->id == -1) {
		bucket->id = id;
		bucket->data = data;
		++htable->_count;
		if (htable->_mask * DYCO_HTABLE_MAXALPHA == htable->_count) {
			if (_htable_resize(htable, htable->_width + 1) < 0) {
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
	newnode->id = id;
	newnode->data = data;
	newnode->next = bucket->next;
	bucket->next = newnode;
	++htable->_count;
	if (htable->_mask * DYCO_HTABLE_MAXALPHA == htable->_count) {
		if (_htable_resize(htable, htable->_width + 1) < 0) {
			return -1;
		}
	}
	return 1;
}

static int
_htable_delete(dyco_htable *htable, int id, void **data)
{
	int idx = id & htable->_mask;
	dyco_hentry *bucket = &htable->_table[idx];
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
		--htable->_count;
		return 0;
	}
	dyco_hentry *pre = bucket;
	while (ptr != NULL) {
		if (ptr->id == id) {
			if (data != NULL)
				*data = ptr->data;
			pre->next = ptr->next;
			free(ptr);
			--htable->_count;
			return 0;
		}
		pre = ptr;
		ptr = ptr->next;
	}
	return -1;
}

static int
_htable_delete_with_freecb(dyco_htable *htable, int id, void (*freecb)(void*))
{
	int idx = id & htable->_mask;
	dyco_hentry *bucket = &htable->_table[idx];
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
		--htable->_count;
		return 0;
	}
	dyco_hentry *pre = bucket;
	while (ptr != NULL) {
		if (ptr->id == id) {
			pre->next = ptr->next;
			if (ptr->data != NULL)
				freecb(ptr->data);
			free(ptr);
			--htable->_count;
			return 0;
		}
		pre = ptr;
		ptr = ptr->next;
	}
	return -1;
}

static void*
_htable_find(dyco_htable *htable, int id)
{
	int idx = id & htable->_mask;
	dyco_hentry *bucket = &htable->_table[idx];
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
_htable_contains(dyco_htable *htable, int id)
{
	int idx = id & htable->_mask;
	dyco_hentry *bucket = &htable->_table[idx];
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
_htable_resize(dyco_htable *htable, int width)
{
	if ((width == htable->_width) || (width > DYCO_HTABLE_MAXWITDH))
		return -1;
	int _newsize = (1 << width);
	int _newmask = _newsize - 1;
	dyco_hentry* _newtable = (dyco_hentry*)calloc(_newsize, sizeof(dyco_hentry));
	int i;
	for (i = 0; i < _newsize; i++) {
		_newtable->id = -1;
		_newtable->data = NULL;
		_newtable->next = NULL;
	}

	int _oldmask = htable->_mask;
	dyco_hentry* _oldtable = htable->_table;
	dyco_hentry *newnode, *next, *ptr;
	int _newidx;
	for (i = 0; i <= _oldmask; i++) {
		if (_oldtable[i].id == -1) {
			continue;
		}
		// first node
		_newidx = _oldtable[i].id & _newmask;
		if (_newtable[_newidx].id == -1) {
			_newtable[_newidx].id = _oldtable[i].id;
			_newtable[_newidx].data = _oldtable[i].data;
		} else {
			newnode = (dyco_hentry*)malloc(sizeof(dyco_hentry));
			newnode->id = _oldtable[i].id;
			newnode->data = _oldtable[i].data;
			newnode->next = _newtable[_newidx].next;
			_newtable[_newidx].next = newnode;
		}
		// other node
		ptr = _oldtable[i].next;
		while (ptr != NULL) {
			next = ptr->next;
			_newidx = ptr->id & _newmask;
			if (_newtable[_newidx].id == -1) {
				_newtable[_newidx].id = ptr->id;
				_newtable[_newidx].data = ptr->data;
				free(ptr);
			} else {
				ptr->next = _newtable[_newidx].next;
				_newtable[_newidx].next = ptr;
			}
			ptr = next;
		}
	}
	free(_oldtable);
	htable->_mask = _newmask;
	htable->_width = width;
	htable->_table = _newtable;
	return 0;
}

static void
_htable_free(dyco_htable *htable)
{
	if (htable == NULL)
		return;

	_htable_clear(htable);
	free(htable);
	return;
}

#endif