#include "redis.h"

dictType dictObjectType = {
	dictStringObjectHashFuction,
	stringObjectDup,
	NULL,
	keyStringObjectCompare,
	stringObjectDestructor,
	NULL
};

robj *createObject(int type, void *ptr)
{
	robj *o = (robj *)zmalloc(sizeof(robj));
	o->type = type;
	o->ptr = ptr;
	o->refcount = 1;

	return o;
}

robj *createStringObject(char *ptr)
{
	char *p = sdsNew(ptr);
	
	return createObject(REDIS_STRING, p);
}

robj *createListObject()
{
	list *l = listCreate();
	robj *o = createObject(REDIS_LIST, l);
	listSetFreeMethod(l, decrRefCountVoid);

	return o;
}

robj *createHashObject()
{
	dict *d = dictCreate(NULL, NULL);
	robj *o = createObject(REDIS_HASH, d);
	
	return o;
}


void freeStringObject(void *o) 
{
	robj *val = (robj *)o;
	assert(val->type == REDIS_STRING);
	sdsFree((sds)val->ptr);
	zfree(val);
}

void freeListObject(void *o)
{
	robj *val = (robj *)o;
	assert(val->type == REDIS_LIST);
	listRelease((list *)val->ptr);
	zfree(val);
}

void freeHashObject(void *o)
{
	robj *val = (robj *)o;
	assert(val->type == REDIS_HASH);
	dictRelease((dict *)val->ptr);
	zfree(val);
}

void decrRefCount(robj *o)
{
	
	if (o->refcount > 1) {
		--o->refcount;
	}
	else if (o->refcount == 1) {
		switch (o->type) {
		case REDIS_STRING:
			freeStringObject(o);
			break;
		case REDIS_LIST:
			freeListObject(o);
			break;
		case REDIS_HASH:
			freeHashObject(o);
			break;
		default:
			assert(0);
		}
	}
}

void incrRefCount(robj *o)
{
	++o->refcount;
}

void decrRefCountVoid(void *o)
{
	decrRefCount((robj *)o);
}

//return 0, if k1 != k2; 
int keyStringObjectCompare(void *privdata, const void *key1, const void *key2)
{
	//if (key1 == NULL || key2 == NULL)
	//	return 0;

	sds k1 = (sds)((robj *)key1)->ptr;
	sds k2 = (sds)((robj *)key2)->ptr;
	int cmp;
	int len1 = sdslen(k1);
	int len2 = sdslen(k2);
	if (len1 != len2)
		return 0;
	
	cmp = memcmp(k1, k2, len1);
	
	return cmp ? 0 : 1;
}

static uint32_t dict_hash_function_seed = 5381;
unsigned int dictStringObjectHashFuction(const void *key)
{	
	const char *p = (sds)((robj *)key)->ptr;

	unsigned int hash = (unsigned int)dict_hash_function_seed;
	unsigned len = strlen(p);
	while (len--) {
		hash = ((hash << 5) + hash) + (tolower(*p++));
	}

	return hash;
}

void *stringObjectDup(void *pirvdata, const void *key)
{
	assert(((robj *)key)->type == REDIS_STRING);
	
	char *s = (char *)(((robj *)key)->ptr);
	robj *k = createStringObject(s);

	return k;
}

void stringObjectDestructor(void *privdata, void *key)
{
	freeStringObject(key);
}

