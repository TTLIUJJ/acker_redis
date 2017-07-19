#pragma once
#ifndef __DICT_H_
#define __DICT_H_

#include <stdint.h>

#define DICT_OK 0
#define DICT_ERR 1


typedef struct dictEntry {
	void *key;
	void *val;
	struct dictEntry *next;
}dictEntry;

typedef struct dictht {
	dictEntry **table;
	unsigned long used;
	unsigned long size;
	unsigned long size_mask;	//size_mask == size-1
}dictht;

typedef struct dictType {
	unsigned int(*hashFunction)(const void *key);
	void *(*keyDup)(void *privdata, const void *key);
	void *(*valDup)(void *privdata, const void *val);
	int(*keyCompare)(void *privdata, const void *key1, const void *key2);
	void(*keyDestructor)(void *privdata, void *key);
	void(*valDestructor)(void *privdata, void *val);
}dictType;

typedef struct dict {
	dictType *type;
	void *privdata;
	dictht ht[2];
	int rehashidx;
	int iterator;
}dict;


typedef struct dictIterator {
	dict *d;
	long index;
	int table;
	int safe;
	dictEntry *entry, *nextEntry;
}dictIterator;


#define DICT_HT_INITIAL_SIZE 4
#define dictHashKey(d, key) ((d)->type->hashFunction(key))
#define dictGetKey(d) ((d)->key)
#define dictGetVal(d) ((d)->val)
#define dictIsRehashing(d) ((d)->rehashidx != -1)
#define dictSlots(d) ((d)->ht[0].size + (d)->ht[1].size)
#define dictSize(d) ((d)->ht[0].used + (d)->ht[1].used)
#define dictCompareKeys(d, key1, key2)\
		(((d)->type->keyCompare) ? (d)->type->keyCompare((d)->privdata, key1, key2) : (key1) == (key2) )

#define dictSetKey(d, de, _key)\
		do{\
			if((d)->type->keyDup)\
				de->key = (d)->type->keyDup((d)->privdata, _key);\
			else\
				de->key = (_key);\
		}while(0)

#define dictFreeKey(d, de)\
		if((d)->type->keyDestructor)\
			(d)->type->keyDestructor((d)->privdata, (de)->key);

#define dictSetVal(d, de, _val)\
		do{\
			if((d)->type->valDup)\
				de->val = (d)->type->valDup((d)->privdata, _val);\
			else\
				de->val = (_val);\
		}\
		while(0)

#define dictFreeVal(d, de)\
		if((d)->type->valDestructor)\
			(d)->type->valDestructor((d)->privdata, (de)->val);

#define dictSetKeyCompareMethod(d, m) (d)->type->keyCompare = (m)			

dict *dictCreate(dictType *type, void *privdata);
dictEntry* dictAddRaw(dict *d, void *key);
dictEntry *dictFind(dict *d, const void *key);
void *dictFetchVal(dict *d, const void *key);
dictIterator *dictGetSafeIterator(dict *d);
dictEntry *dictNext(dictIterator *iter);
void dictReleaseIterator(dictIterator *iter);
int dictDelete(dict *d, const void *key);
void dictRelease(dict *d);
unsigned int dictGenCaseHashFunction(const void *buf);
int dictAdd(dict *d, void *key, void *val);
int dictExpand(dict *d, unsigned long size);
int dictRehash(dict *d, int n);
int dictSdsCompare(void *privdata, const void *key1, const void *key2);


#endif 