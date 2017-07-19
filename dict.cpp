#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>
#include "zmalloc.h"
#include "dict.h"

static int dict_can_resize = 1;
static unsigned int dict_force_resize_ratio = 5;
static uint32_t dict_hash_function_seed = 5381;

static int _dictInit(dict *d, dictType *type, void *privdata);
static void _dictReset(dictht *ht);
static int _dictKeyIndex(dict *d, const void *key);
static unsigned long _dictNextPower(unsigned long size);



dictType defaultDictType = {
	dictGenCaseHashFunction,
	NULL,
	NULL,
	dictSdsCompare,
	NULL,
	NULL
};



static int _dictInit(dict *d, dictType *type, void *privdata)
{
	_dictReset(&d->ht[0]);
	_dictReset(&d->ht[1]);
	d->privdata = privdata;
	d->rehashidx = -1;
	d->iterator = 0;
	if (type == NULL) {
		d->type = &defaultDictType;
	}
	else
		d->type = type;
	
	return DICT_OK;
}

static void _dictReset(dictht *ht)
{
	ht->size = 0;
	ht->size_mask = 0;
	ht->used = 0;
	ht->table = NULL;
}

int _dictExpandIfNeeded(dict *d)
{
	if (dictIsRehashing(d))
		return DICT_ERR;

	if (d->ht[0].size == 0)
		return dictExpand(d, DICT_HT_INITIAL_SIZE);

	unsigned int ratio = d->ht[0].used / d->ht[0].size;
	if (d->ht[0].used >= d->ht[0].size && (dict_can_resize || ratio > dict_force_resize_ratio))
		return dictExpand(d, d->ht[0].used * 2);

	return DICT_OK;
}

static int _dictKeyIndex(dict *d, const void *key)
{
	unsigned int h, idx, table;
	dictEntry *de;
		
	_dictExpandIfNeeded(d);
	
	h = dictHashKey(d, key);
	
	for (table = 0; table < 2; ++table) {
		idx = h & d->ht[table].size_mask;
		de = d->ht[table].table[idx];
		while (de != NULL) {
			if (dictCompareKeys(d, key, de->key))
				return -1;
			
			de = de->next;
		}

		if (!dictIsRehashing(d))
			break;
	}
	
	return idx;
}

static unsigned long _dictNextPower(unsigned long size)
{
	unsigned long i = DICT_HT_INITIAL_SIZE;

	if (size >= LONG_MAX)
		return LONG_MAX;
	
	while (1) {
		if (i >= size)
			return i;
		i *= 2;
	}
}

void _dictRehashStep(dict *d)
{
	if (d->iterator == 0)
		dictRehash(d, 1);
}

//往哈希表添加节点
//返回NULL若节点已存在
dictEntry* dictAddRaw(dict *d, void *key)
{
	int index;
	dictht *ht;

	if (dictIsRehashing(d))
		_dictRehashStep(d);
	
	if ((index = _dictKeyIndex(d, key)) == -1)
		return NULL;
	
	ht = (dictIsRehashing(d)) ? &d->ht[1] : &d->ht[0];
	dictEntry *de = (dictEntry *)zmalloc(sizeof(struct dictEntry));
	de->next = ht->table[index];
	ht->table[index] = de;
	++ht->used;
	
	dictSetKey(d, de, key);
	
	return de;
}

//创建字典
dict *dictCreate(dictType *type, void *privdata)
{
	dict *d = (struct dict *)zmalloc(sizeof(struct dict));
	_dictInit(d, type, privdata);

	return d;
}

int dictExpand(dict *d, unsigned long size)
{
	dictht ht;
	unsigned long realsize = _dictNextPower(size);

	if (dictIsRehashing(d) || d->ht[0].used > size)
		return DICT_ERR;

	if (realsize == d->ht[0].size)
		return DICT_ERR;

	ht.size = realsize;
	ht.size_mask = realsize - 1;
	ht.table = (dictEntry **)(zcalloc(realsize * sizeof(dictEntry *)));
	ht.used = 0;

	if (d->ht[0].table == NULL) {
		d->ht[0] = ht;
		return DICT_OK;
	}
	d->ht[1] = ht;
	d->rehashidx = 0;	//ready for rehashing

	return DICT_OK;
}

//return 0表示rehash已经结束
int dictRehash(dict *d, int n)
{
	int empty_visits = n * 10;
	if (!dictIsRehashing(d))
		return 0;
	
	while (n-- && d->ht[0].used != 0) {
		dictEntry *de, *nextde;

		assert(d->ht[0].size > (unsigned long)d->rehashidx);
		while (d->ht[0].table[d->rehashidx] == NULL) {
			++d->rehashidx;
			if (--empty_visits == 0)
				return 1;
		}
		de = d->ht[0].table[d->rehashidx];
		while (de) {
			unsigned int h;
			nextde = de->next;

			h = dictHashKey(d, de->key) & d->ht[1].size_mask;
			de->next = d->ht[1].table[h];
			d->ht[1].table[h] = de;
			--d->ht[0].used;
			++d->ht[1].used;
			de = nextde;
		}
		d->ht[0].table[d->rehashidx] = NULL;
		++d->rehashidx;
	}
	if (d->ht[0].used == 0) {
		zfree(d->ht[0].table);
		d->ht[0] = d->ht[1];
		_dictReset(&d->ht[1]);
		d->rehashidx = -1;
		return 0;
	}

	return 1;
}

//通过键获取哈希表节点
//若不存在返回NULL
dictEntry *dictFind(dict *d, const void *key)
{
	dictEntry *de;
	unsigned int h, idx, table;
	
	if (d->ht[0].size == 0)
		return NULL;

	if (dictIsRehashing(d))
		_dictRehashStep(d);
	h = d->type->hashFunction(key);
	
	for (table = 0; table < 2; ++table) {
		idx = h & d->ht[table].size_mask;
		de = d->ht[table].table[idx];
		while (de) {
			if (dictCompareKeys(d, key, de->key)) {
				return de;
			}
			de = de->next;
		}
		if (!dictIsRehashing(d))
			break;
	}
	
	return NULL;
}

//获取哈希节点的值
//若键没有对应的值，返回NULL
void *dictFetchVal(dict *d, const void *key)
{
	dictEntry *de;

	de = dictFind(d, key);
	return de ? dictGetVal(de) : NULL;
}

dictIterator *dictGetIterator(dict *d)
{
	dictIterator * iter = (dictIterator *)zmalloc(sizeof(struct dictIterator));
	if (iter == NULL)
		return NULL;

	iter->d = d;
	iter->table = 0;
	iter->index = -1;
	iter->safe = 0;
	iter->entry = NULL;
	iter->nextEntry = NULL;

	return iter;
}

//获得安全的字典迭代器
dictIterator *dictGetSafeIterator(dict *d)
{
	dictIterator *i = dictGetIterator(d);

	i->safe = 1;
	return i;
}

//通过迭代器找下一个已存在的哈希节点
dictEntry *dictNext(dictIterator *iter)
{
	while (1) {
		if (iter->entry == NULL) {
			dictht *ht = &iter->d->ht[iter->table];
			if (iter->index == -1 && iter->table == 0) {
				if (iter->safe)
					++iter->d->iterator;
			}
			++iter->index;
			if (iter->index >= (long)ht->size) {
				if (dictIsRehashing(iter->d) && iter->table == 0) {
					++iter->table;
					iter->index = 0;
					ht = &iter->d->ht[1];
				}
				else
					break;
			}
			iter->entry = ht->table[iter->index];
		}
		else {
			iter->entry = iter->nextEntry;
		}

		if (iter->entry) {
			iter->nextEntry = iter->entry->next;
			return iter->entry;
		}
	}

	return NULL;
}

void dictReleaseIterator(dictIterator *iter)
{
	if (!(iter->index == -1 && iter->table == 0)){
		if (iter->safe)
			--iter->d->iterator;
	}

	zfree(iter);
}

static int dictGenericDelete(dict *d, const void *key, int nofree)
{
	unsigned int h, idx;
	dictEntry *de, *prev;
	int table;

	if (d->ht[0].size == 0)
		return DICT_ERR;
	if (dictIsRehashing(d))
		_dictRehashStep(d);
	h = dictHashKey(d, key);

	for (table = 0; table < 2; ++table) {
		idx = h & d->ht[table].size_mask;
		de = d->ht[table].table[idx];
		prev = NULL;
		while (de) {
			if (dictCompareKeys(d, key, de->key)) {
				if (prev) {
					prev->next = de->next;
				}
				else {
					d->ht[table].table[idx] = de->next;
				}
				if (!nofree) {
					dictFreeKey(d, de);
					dictFreeVal(d, de);
				}
				zfree(de);
				--d->ht[table].used;
				return DICT_OK;
			}
			prev = de;
			de = de->next;
		}
		if (!dictIsRehashing(d))
			break;
	}
	return DICT_ERR;
}

//删除并释放哈希节点
int dictDelete(dict *d, const void *key)
{
	return dictGenericDelete(d, key, 0);
}

int _dictClear(dict *d, dictht*ht, void(callback)(void *))
{
	unsigned long i;

	for (i = 0; i < ht->size && ht->used >0; ++i) {
		dictEntry *de, *nextde;
		if (callback && (i & 65536) == 0)
			callback(d->privdata);
		if ((de = ht->table[i]) == NULL)
			continue;
		while (de) {
			nextde = de->next;
			dictFreeKey(d, de);
			dictFreeVal(d, de);
			zfree(de);
			--ht->used;
			de = nextde;
		}
	}
	zfree(ht->table);
	_dictReset(ht);

	return DICT_OK;
}

//释放字典
void dictRelease(dict *d)
{
	_dictClear(d, &d->ht[0], NULL);
	_dictClear(d, &d->ht[1], NULL);
	zfree(d);
}

unsigned int dictGenCaseHashFunction(const void *buf)
{
	const char *p = (const char *)buf;
	unsigned int hash = (unsigned int)dict_hash_function_seed;
	unsigned len = strlen(p);
	while (len--) {
		hash = ((hash << 5) + hash) + (tolower(*p++));
	}
	
	return hash;
}

int dictAdd(dict *d, void *key, void *val)
{
	dictEntry *de = dictAddRaw(d, key);
	
	if (!de)
		return DICT_ERR;
	dictSetVal(d, de, val);
	
	return DICT_OK;
}

//if ke1==key2 return 1
int dictSdsCompare(void *privdata, const void *key1, const void *key2)
{
	char *k1 = (char *)key1;
	char *k2 = (char *)key2;

	int len1 = strlen(k1);
	int len2 = strlen(k2);
	if (len1 != len2)
		return 0;

	int cmp = memcmp(key1, key2, len1);
	if (cmp == 0)
		return 1;
	else
		return 0;
}

