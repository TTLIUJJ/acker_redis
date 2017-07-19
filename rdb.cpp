#include "redis.h"

#define SIZEBUF 16
#define SDSBUF (64 * 1024)
#define EXCEPT_SDSLEN(len) ((len <= 0) && (len > SDSBUF))
#define REDIS_IS_DIGIT(c) (((c) <= '9') && ((c) >= '0') )

static void parseSdsSize(int val, char *buf);
static size_t compressLenToRDB(size_t slen, char *buf, size_t *pos);
static size_t parseSdsIntVal(char *buf, size_t *pos);

//the compresss format is like that, no incluing ' ', just for reading comfortably.
//the rule is that: it includes '.' after every string length and the length of list or the pairs of hash
//this is an example of rdb file.
//REDIS 1 5.string3.yes 2 4.list 3. 1.a 2.b 3.c
//		3 4.hash 2. 3.abc 2.de 3.xxx 3.yyy
size_t startCompressRDB(redisDb *db, char *buf, size_t *pos)
{
	dict *d = db->dict;
	dictIterator *iter = dictGetSafeIterator(d);
	dictEntry *de;
	char sizebuf[SIZEBUF];
	//so. At first the "REDIS" should be compressed
	//it is a symbolof rdb file.
	memcpy(buf, "REDIS", 5);
	*pos += 5;
	size_t totlen = 5;
	while ((de = dictNext(iter)) != NULL) {
		totlen += compressToRDB(de, buf, pos);
	}
	//length totlen is excluded in totlen.
	//the stand of eof is "$$"
	//and last compressing the 'totlen'
	memcpy(buf + *pos, "$$", 2);
	*pos += 2;
	totlen += 2;
	parseSdsSize(totlen, sizebuf);
	memcpy(buf + *pos, sizebuf, strlen(sizebuf));
	size_t tlen = strlen(sizebuf);
	*pos += tlen;
	totlen += tlen;
	memset(buf + *pos, 0, 1);
	return totlen;
}

//compress every entry of dictht.
// in the db, (key - val) both of them are objects 
//return the size of dictEntry. and you konw both k-v is an object

size_t compressToRDB(dictEntry *de, char *buf, size_t *pos)
{
	size_t keysize;
	size_t valsize;
	robj *key = (robj *)de->key;
	robj *val = (robj *)de->val;
	
	switch (val->type) {
	case REDIS_STRING:
		keysize = compressKeyToRDB(key, buf, pos, REDIS_STRING);
		valsize = compressStringValueToRDB(val, buf, pos);
		break;
	case REDIS_LIST:
		keysize = compressKeyToRDB(key, buf, pos, REDIS_LIST);
		valsize = compressListValueToRDB(val, buf, pos);
		break;
	case REDIS_HASH:
		keysize = compressKeyToRDB(key, buf, pos, REDIS_HASH);
		valsize = compressHashValueToRDB(val, buf, pos);
		break;
	default:
		assert(0);
	}
	// extra 1 byte for type
	return keysize + valsize + 1;
}

//in first step, we should compress the size of sds
//and each sds of length end of '.'
size_t compressSdsToRDB(sds s, char *buf, size_t *pos)
{	
	struct sdshdr *sh = (struct sdshdr *)(s - sizeof(struct sdshdr));
	size_t sdslen = sh->len;
	size_t sizelen;
	char sizebuf[SIZEBUF];
	
	parseSdsSize(sdslen, sizebuf);
	sizelen = strlen(sizebuf);
	memcpy(buf + *pos, sizebuf, sizelen);
	*pos += sizelen;
	memcpy(buf + *pos, sh->buf, sdslen);
	*pos += sdslen;
	return sdslen + sizelen;
}

//at first, we must compress the type
//and a key always is an string object.
size_t compressKeyToRDB(robj *key, char *buf, size_t *pos, int type)
{
	switch (type) {
	case REDIS_STRING:
		memset(buf + *pos, '1', 1);
		break;
	case REDIS_LIST:
		memset(buf + *pos, '2', 1);
		break;
	case REDIS_HASH:
		memset(buf + *pos, '3', 1);
		break;
	default:
		assert(0);
	}
	++*pos;
	return compressStringValueToRDB(key, buf, pos);
}

size_t compressStringValueToRDB(robj *val, char *buf, size_t *pos)
{
	sds s = (sds)val->ptr;
	return compressSdsToRDB(s, buf, pos);
}

//now , supposed that the node of list is sds
//actually, we can have three optionals, but it is complex
//and it is the same as hsah
size_t compressListValueToRDB(robj *val, char *buf, size_t *pos)
{
	size_t len = 0;
	list *l = (list *)val->ptr;
	listIterator *iter = listGetIterator(l, AL_START_HEAD);
	listNode *node;

	size_t slen = l->len;
	len = compressLenToRDB(slen, buf, pos);
	*pos += len;
	
	while ((node = listNext(iter)) != NULL) {
		robj *s = (robj *)(node->value);
		len += compressStringValueToRDB(s, buf, pos);
	}
	listReleaseIterator(iter);

	return len;
}

//in dict, (key - val) both of them are sds.
//this situation is different from above.
size_t compressHashValueToRDB(robj *val, char *buf, size_t *pos)
{
	size_t len = 0;
	dict *d = (dict *)val->ptr;

	//Secondly, compressing the size of dictentry used. 
	size_t slen = d->ht[0].used + d->ht[1].used;
	len = compressLenToRDB(slen, buf, pos);
	*pos += len;

	dictIterator *iter = dictGetSafeIterator(d);
	dictEntry *de;

	while ((de = dictNext(iter)) != NULL) {
		robj* key = (robj*)de->key;
		robj* val = (robj*)de->val;
		len += compressStringValueToRDB(key, buf, pos);
		len += compressStringValueToRDB(val, buf, pos);
	}
	dictReleaseIterator(iter);

	return len;
}

static void parseSdsSize(int val, char *buf)
{
	int i;
	for (i = 0; i < SIZEBUF - 2; ++i) {
		if (!val)
			break;
		int aux = val % 10;
		val /= 10;
		char p = '0' + aux;
		memset(buf + i, p, 1);
	}
	memset(buf + i, 0, 1);

	int len = strlen(buf);
	char *e = buf + len - 1;
	char *s = buf;
	char c;
	while (s < e) {
		c = *s;
		*s = *e;
		*e = c;
		++s;
		--e;
	}
	memset(buf + i, '.', 1);
	memset(buf + i + 1, '\0', 1);
}

static size_t compressLenToRDB(size_t slen, char *buf, size_t *pos)
{
	char sizebuf[SIZEBUF];
	parseSdsSize(slen, sizebuf);	
	size_t len = strlen(sizebuf);
	memcpy(buf + *pos, sizebuf, len);
	return len;
}

// ============================  Load From RDB File ========================

void startLoadFromRDB(redisClient *client, char *buf, size_t *pos)
{
	dict *d = client->db->dict;
	size_t totlen = 0;
	size_t buflen = strlen(buf);
	if(*buf != 'R' || *(buf + 1) != 'E' || *(buf + 2) != 'D' ||
			*(buf + 3) != 'I' || *(buf + 4) != 'S'){
		perror("load error of rdb file: ");
		exit(1);
	}
	*pos += 5;
	
	
	while (totlen < buflen) {
		totlen += *pos;
		if (*(buf + *pos) == '$' && *(buf + *pos + 1) == '$')
			break;
		extractFromRDB(d, buf, pos);
	}
	totlen += 2;
	
	
}

void extractFromRDB(dict *d ,char *buf, size_t *pos)
{
	size_t totlen = 0;
	char c = *(buf + *pos);
	int type = c - '0';
	++*pos;
	robj *key = extractStringObjectFromRDB(buf, pos);
	robj *val;
	
	switch (type) {
	case REDIS_STRING:
		val = extractStringObjectFromRDB(buf, pos);
		break;
	case REDIS_LIST:
		val = extractListObjectFromRDB(buf, pos);
		break;
	case REDIS_HASH:
		val = extractHashObjectFromRDB(buf, pos);
		break;
	default:
		assert(0);
	}
	dictAdd(d, key, val);
}

size_t extractFromSdsLen(char *buf, size_t *pos)
{
	size_t slen;

	char c = *(buf + (*pos));
	if (REDIS_IS_DIGIT(c)) {
		slen = parseSdsIntVal(buf, pos);
		return slen;
	}
	
	return 0;
}

sds extractSdsFromRDB(char *buf, size_t *pos)
{
	char p[SDSBUF];
	size_t slen = extractFromSdsLen(buf, pos);

	if (!slen && slen >= SDSBUF){
		perror("extractSdsFromRDB error:");
		exit(1);
	}
	memcpy(p, buf + *pos, slen);
	*pos += slen;
	sds s = sdsNewlen(p, slen);

	return s;
}

robj *extractStringObjectFromRDB(char *buf, size_t *pos)
{
	sds p = extractSdsFromRDB(buf, pos);
	robj *o = createStringObject(p);
	
	return o;
}

//memmory leak!!!!!!!!!!!!!!
//list is created twice.
robj *extractListObjectFromRDB(char *buf, size_t *pos)
{
	robj *o = createListObject();
	list *l = listCreate();

	size_t slen = parseSdsIntVal(buf, pos);
	((list *)o->ptr)->len = slen;

	while (slen--) {
		robj* s = extractStringObjectFromRDB(buf, pos);
		listAddNodeHead(l, s);
	}
	o->ptr = l;

	return o;
}

//memeory leak!!!!!!!!!!!!!!!!!!!
robj *extractHashObjectFromRDB(char *buf, size_t *pos)
{
	robj *o = createHashObject();
	dict *d = dictCreate(&dictObjectType, NULL);

	size_t slen = parseSdsIntVal(buf, pos);
	while (slen--) {
		robj* key = extractStringObjectFromRDB(buf, pos);
		robj* val = extractStringObjectFromRDB(buf, pos);

		dictAdd(d, key, val);
	}
	o->ptr = d;

	return o;
}

static size_t parseSdsIntVal(char *buf, size_t *pos)
{
	size_t len, ret = 0;
	char *point = strchr(buf + *pos, '.');
	char *head = buf + *pos;
	if (point == NULL)
		return 0;
	else if (point - head > 14)
		return 0;

	len = point - head;
	size_t p = 1;
	size_t i = len - 1;
	while (i-- > 0) {
		p *= 10;
	}

	while (len--) {
		char c = *(buf + *pos);
		++(*pos);
		int i = c - '0';
		ret = ret + i * p;
		p /= 10;
		if (p == 0)
			p = 1;
	}
	//skip point '.'
	++*pos;

	return ret;
}
