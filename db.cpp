#include "redis.h"

//return NULL if the key doesn't in redisDb
//else return the dictEntry in db
//now. it just incrrefcount for the string object.
dictEntry *lookupKey(redisDb *db, robj *key)
{
	dict *d = (dict *)db->dict;
	dictEntry *de = dictFind(d, key);

	if (de == NULL)
		return NULL;
	
	return de;
}

int dbModifyStringVal(redisDb *db, void *key, void *val)
{
	dict *d = db->dict;
	dictEntry *de = dictFind(d, key);
	if (de == NULL) 
		return REDIS_ERR;

	robj *old_val = (robj *)de->val;
	robj *new_val = (robj *)val;
	if (keyStringObjectCompare(NULL, old_val, new_val))
		return REDIS_ERR;

	decrRefCount(old_val);
	de->val = new_val;

	return REDIS_OK;
}

//remove a string-string hash key-val
//but the situation of bucket may be included one more dictEntry. 
void dbRemoveHashEntry(redisDb *db, void *key, void *val)
{
	removeHashEntry(db->dict, key, val);
}

void removeHashEntry(dict *d, void *key, void *val)
{
	unsigned int h, idx, table;
	dictEntry *cur, *prev;
	int decr = 0;
	h = (d->type->hashFunction)(key);
	for (table = 0; table < 2; ++table) {
		idx = h & d->ht[table].size_mask;
		cur = d->ht[table].table[idx];
		prev = NULL;
		if (cur != NULL) {
			if (dictCompareKeys(d, key, cur->key)) {
				d->ht[table].table[idx] = cur->next;
				decr = 1;
				break;
			}
			prev = cur;
			cur = cur->next;
		}
		while (cur != NULL) {
			if (dictCompareKeys(d, key, cur->key)) {
				prev->next = cur->next;
				decr = 1;
				break;
			}
			prev = cur;
			cur = cur->next;
		}
		if (dictIsRehashing(d))
			break;
	}
	if (decr) {
		--d->ht[table].used;
		decrRefCountVoid(key);
		decrRefCountVoid(val);
	}
}