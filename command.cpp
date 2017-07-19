#include "redis.h"

#define EXPECT_QUERY(diff) assert(diff < MAXQUERY)
#define NOT_FOUND(client) \
		do{\
			memcpy(client->buf + client->bufpos, "(not found)", 11);\
			client->bufpos += 11;\
		}\
		while(0)

#define EXIST_FOR_ANTHOR_OBJECT(client, p)\
		do{\
			memcpy(client->buf + client->bufpos, p, strlen(p));\
			client->bufpos += strlen(p);\
			client->buf[client->bufpos] = '\0';\
		}while(0)\



// ===================  C  O  M  M  A  N  N  D  ===========================
static void explainNotForThat(redisClient *client, int type);
static int parseObjectToDigit(robj *o);
static void parseDigitToString(int len, char *buf);
//using free() , unless it would free repeat.
dict *initCommandTable()
{
	char k1[4] = "get";
	char k2[4] = "set";
	char k3[4] = "del";
	char k4[6] = "lpush";
	char k5[5] = "lpop";
	char k6[7] = "lindex";
	char k7[5] = "hget";
	char k8[5] = "hset";
	char k9[5] = "hlen";
	char k10[5]= "type";
	char k11[3] = "db";
	redisCommand cmdtable[11] = {
		{ k1, 2, &getCommand },
		{ k2, 3, &setCommand },
		{ k3, 2, &delCommand },
		{ k4, -3, &lpushCommand },
		{ k5, 2, &lpopCommand },
		{ k6, 3, &lindexCommand },
		{ k7, 3, &hgetCommand },
		{ k8, -4 ,&hsetCommand },
		{ k9, 2 ,&hlenCommand },
		{ k10, 2, &typeCommand },
		{ k11, 3, &dbCommand},
	};
	dict *d = dictCreate(NULL, NULL);

	for (size_t i = 0; i < 11; ++i) {
		redisCommand cmd = cmdtable[i];
		sds k = sdsNew(cmd.name);
		redisCommand *v = (redisCommand *)zmalloc(sizeof(struct redisCommand));
		v->arbity = cmd.arbity;
		v->name = k;
		v->proc = cmd.proc;

		dictAdd(d, k, v);
	}
	printf("========= After init CommandTabel =========\n");
	return d;
}

static int parseObjectToDigit(robj *o)
{
	int ret = 0;
	char s[16];
	int index = 1;

	sds buf = (sds)o->ptr;
	int len = sdslen(buf);
	memcpy(s, buf, len);
	char *p = s + len;

	if (*s == '-')
		--len;

	while (len--) {
		--p;
		int i = *p - '0';
		ret = ret + i * index;
		index *= 10;
	}
	if (*s == '-') {
		return -ret;
	}
	return ret;
}

static void parseDigitToString(int len, char *b)
{
	//example   123
	int i = 0, j = 0;
	char rbuf[16];

	while (len > 0) {
		char c = (len % 10) + '0';
		rbuf[i] = c;
		++i;
		len /= 10;
	}
	char *p = rbuf + i;
	while (i--) {
		--p;
		b[j] = *p;
		++j;
	}
	memset(b + j, 0, 1);
}

static void explainNotForThat(redisClient *client, int type)
{
	char s[] = "(Exist for a String Object)";
	char l[] = "(Exist for a List Object)";
	char h[] = "(Exist for a Hash Object)";

	switch (type) {
	case REDIS_STRING:
		EXIST_FOR_ANTHOR_OBJECT(client, s);
		break;
	case REDIS_LIST:
		EXIST_FOR_ANTHOR_OBJECT(client, l);
		break;
	case REDIS_HASH:
		EXIST_FOR_ANTHOR_OBJECT(client, h);
		break;
	default:
		assert(0);
	}
}


void getCommand(redisClient *client)
{
	dict *d = client->db->dict;
	robj *o = client->argv[1];
	dictEntry *de = dictFind(d, o);

	if (de != NULL) {
		robj *val = (robj *)de->val;
		if (val->type == REDIS_STRING) {
			size_t len = sdslen((sds)val->ptr);
			memcpy(client->buf + client->bufpos, sds(val->ptr), len);
			memset(client->buf+len, 0, 1);
			client->bufpos += len;
		}
		else {
			explainNotForThat(client, val->type);
			return;
		}
	}
	else {
		memcpy(client->buf + client->bufpos, "(not found)\n\0", 13);
		client->bufpos += 13;
	}
}

//important attention..  actually the string object in redisDb will be shared
//									if all of them are equal
//						According to this strategry. it will scan all key in db->dict
//						before creating a stringObject. 
//						It is not the best plan for save memmory, but it save cpu time.
void setCommand(redisClient *client)
{
	dict *d = client->db->dict;
	robj *o = client->argv[1];
	dictEntry *de = dictFind(d, o);

	//three conditions if we			 set A - B
	//and it may be like that in db:		 C - D
	//										 A - B
	//										 A - E
	if (de != NULL) {
		robj *val = (robj *)de->val;
		if (val->type == REDIS_STRING) {
			//if argv[2] == val, it shouldn't be created again or change.
			if (keyStringObjectCompare(NULL, client->argv[2], val)) {
				//do noting.       
			}
			else {
				robj *val2 = createStringObject((char *)(client->argv[2])->ptr);
				if (dbModifyStringVal(client->db, de->key, val2) != REDIS_OK)
					assert(0);
			}
		}
		else {
			explainNotForThat(client, val->type);
			return;
		}
	}
	else {
		char *k = (char *)client->argv[1]->ptr;
		char *v = (char *)client->argv[2]->ptr;
		robj *key = createStringObject(k);
		robj *val = createStringObject(v);
		dictAdd(client->db->dict, key, val);
	}
	memcpy(client->buf + client->bufpos, "(set OK)\n\0", 10);
	client->bufpos += 10;
}

void delCommand(redisClient *client)
{
	dict *d = client->db->dict;
	robj *o = client->argv[1];
	dictEntry *de = dictFind(d, o);

	if (de != NULL) {
		robj *val = (robj*)de->val;
		if (val->type == REDIS_STRING) {
			robj *key = (robj *)de->key;
			dbRemoveHashEntry(client->db, key, val);
		}
		else {
			explainNotForThat(client, val->type);
			return;
		}
	}
	//generally speaking, if the argv[1] cann't find in dictEntry
	//so we can delete it in a wrong buf safe way.
	memcpy(client->buf + client->bufpos, "(del OK)\n\0", 10);
	client->bufpos += 10;
}

//every node in list is a stringObject
void lpushCommand(redisClient *client)
{
	dict *d = client->db->dict;
	robj *o = client->argv[1];
	dictEntry *de = dictFind(d, o);

	robj *key;
	robj *val;
	if (de != NULL) {
		int type = ((robj *)de->val)->type;
		if (type != REDIS_LIST) {
			explainNotForThat(client, type);
			return;
		}
		else {
			key = (robj *)de->key;
			val = (robj *)de->val;
		}
	}
	else {
		key = createStringObject((char *)o->ptr);
		val = createListObject();
		dictAdd(d, key, val);
	}

	for (int i = 2; i < client->argc; ++i) {
		list *l = (list *)val->ptr;
		robj *v = createStringObject((char *)(client->argv[i]->ptr));
		listAddNodeHead(l, v);
	}
	memcpy(client->buf + client->bufpos, "(lpush OK)", 10);
	client->bufpos += 10;
}

void lpopCommand(redisClient *client)
{
	dict *d = client->db->dict;
	robj *o = client->argv[1];
	dictEntry *de = dictFind(d, o);

	if (de != NULL) {
		robj *val = (robj *)de->val;
		if (val->type != REDIS_LIST) {
			explainNotForThat(client, val->type);
		}
		list *l = (list *)val->ptr;
		if (l->len == 0) {
			memcpy(client->buf + client->bufpos, "(empty)", 7);
			client->bufpos += 7;
			return;
		}
		listNode *node = l->head;
		char *p = (sds)((robj *)node->value)->ptr;
		memcpy(client->buf + client->bufpos, p, strlen(p));
		client->bufpos += strlen(p);
		listDelNode(l, node);
	}
	else {
		memcpy(client->buf + client->bufpos, "(no such list)", 14);
		client->bufpos += 14;
	}
}
//index range( -~ , -1] and [1, +~ )
void lindexCommand(redisClient *client)
{
	dict *d = client->db->dict;
	robj *o = client->argv[1];
	dictEntry *de = dictFind(d, o);

	if (de != NULL) {
		robj *val = (robj *)de->val;
		if (val->type != REDIS_LIST) {
			explainNotForThat(client, val->type);
			return;
		}
		list *l = (list *)val->ptr;
		listNode *node;
		int len = parseObjectToDigit(client->argv[2]);
		unsigned long ablen = (len < 0) ? -len : len;
		if (ablen > l->len) {
			memcpy(client->buf + client->bufpos, "(out of range)", 14);
			client->bufpos += 14;
			return;
		}
		if (len < 0) {
			len = -len;
			node = listTail(l);
			while (--len)
				node = node->prev;
		}
		else {
			node = listFirst(l);
			while (--len)
				node = node->next;
		}
		char *p = (char *)((robj *)node->value)->ptr;
		size_t plen = strlen(p);
		memcpy(client->buf + client->bufpos, p, plen);
		client->bufpos += plen;
	}
	else {
		memcpy(client->buf + client->bufpos, "(no such list)", 14);
		client->bufpos += 14;
	}
}
//every dictEntry in nested hash is  (string - string)
//hget book adventure
void hgetCommand(redisClient *client)
{
	dict *d = client->db->dict;
	robj *o = client->argv[1];
	dictEntry *de = dictFind(d, o);

	if (de != NULL) {
		robj *val = (robj *)de->val;
		if (val->type != REDIS_HASH) {
			explainNotForThat(client, val->type);
			return;
		}

		robj *k = client->argv[2];
		dictEntry *nde = dictFind((dict *)val->ptr, k);
		if (nde != NULL) {
			robj *v = (robj *)nde->val;
			char *p = (char *)v->ptr;
			size_t plen = strlen(p);
			memcpy(client->buf + client->bufpos, p, plen);
			client->bufpos += plen;
		}
		else {
			memcpy(client->buf + client->bufpos, "(no such key in dict)", 21);
			client->bufpos += 21;
		}
	}
	else {
		memcpy(client->buf + client->bufpos, "(no such dictory)", 17);
		client->bufpos += 17;
	}
}
#define EXECUTE_ARGC_ERROR(client)\
		do{\
			memcpy(client->buf + client->bufpos, "(argc error)", 12);\
			client->bufpos += 12;\
		}while(0)

void hsetCommand(redisClient *client)
{
	dict *d = client->db->dict;
	robj *o = client->argv[1];
	dictEntry *de = dictFind(d, o);
	//acutally it includes that in hashtable		 			book name  huang
	//it should consider such  situaltions, if we command:  hset book name acker
	//just including right command format					hset x    name acker
	//														hset book weather raining
	if (de != NULL) {
		robj *val = (robj *)de->val;
		if (val->type != REDIS_HASH) {
			explainNotForThat(client, val->type);
			return;
		}
		if (client->argc != 4) {
			EXECUTE_ARGC_ERROR(client);
			return;
		}
		dict *d = (dict *)val->ptr;
		dictEntry *nde = dictFind(d, client->argv[2]);
		robj *nk, *nv;
		if (nde == NULL) {
			nk = createStringObject((char *)(client->argv[2]->ptr));
		}
		else {
			nk = (robj *)nde->key;
			incrRefCount(nk);
			removeHashEntry(d, nde->key, nde->val);
		}
		nv = createStringObject((char *)(client->argv[3]->ptr));
		dictAdd(d, nk, nv);
	}
	else {
		//if argv[1] isn't a dictory, and now we should create a hashObject in db->dict
		//and before process it, it argcs should be checked.
		if ((client->argc % 2) != 0) {
			EXECUTE_ARGC_ERROR(client);
			return;
		}

		robj *noh = createHashObject();
		robj *nks = createStringObject((char *)(client->argv[1])->ptr);
		dict *nd = dictCreate(&dictObjectType, NULL);
		for (int i = 2; i < client->argc; i += 2) {
			char *nk = (char *)(client->argv[i])->ptr;
			char *nv = (char *)(client->argv[i + 1])->ptr;
			robj *nok = createStringObject(nk);
			robj *nov = createStringObject(nv);
			dictAdd(nd, nok, nov);
		}
		noh->ptr = nd;
		dictAdd(d, nks, noh);

	}
	memcpy(client->buf + client->bufpos, "(hset OK)", 9);
	client->bufpos += 9;
}

void hlenCommand(redisClient *client)
{
	dict *d = client->db->dict;
	robj *o = client->argv[1];
	dictEntry *de = dictFind(d, o);

	if (de != NULL) {
		robj *val = (robj *)de->val;
		if (val->type != REDIS_HASH) {
			explainNotForThat(client, val->type);
			return;
		}
		int len = ((dict *)(val->ptr))->ht[0].used + ((dict *)(val->ptr))->ht[1].used;
		char p[16];
		parseDigitToString(len, p);
		size_t plen = strlen(p);
		memcpy(client->buf + client->bufpos, p, plen);
		client->bufpos += plen;
		client->buf[client->bufpos] = '\0';
	}
	else {
		memcpy(client->buf + client->bufpos, "(no such dictory)", 17);
		client->bufpos += 17;
	}
}

void typeCommand(redisClient *client)
{
	dict *d = client->db->dict;
	robj *q = client->argv[1];
	dictEntry *de = dictFind(d, q);

	if (de != NULL) {
		robj *val = (robj *)de->val;
		switch (val->type) {
		case REDIS_STRING:
			memcpy(client->buf + client->bufpos, "string", 6);
			client->bufpos += 6;
			break;
		case REDIS_LIST:
			memcpy(client->buf + client->bufpos, "list", 4);
			client->bufpos += 4;
			break;
		case REDIS_HASH:
			memcpy(client->buf + client->bufpos, "hash", 4);
			client->bufpos += 4;
			break;
		default:
			memcpy(client->buf + client->bufpos, "(none)", 6);
			client->bufpos += 6;
			break;
		}
	}
	else
		NOT_FOUND(client);
}

//now db includes two operations.
//a) db -s filename  save 
//a) db -l filename	 load
static void dbSaveCommand(redisClient *client);
static void dbLoadCommand(redisClient *client);
#define BIG_BUF (1024 * 1024)
void dbCommand(redisClient *client)
{
	robj *o1 = client->argv[1];
	char* cmd1 = (sds)o1->ptr;
	size_t cmdlen = strlen(cmd1);

	if(*cmd1 != '-' || cmdlen != 2){
		NOT_FOUND(client);
		return;
	}
	
	char type = cmd1[1];
	switch(type){
	case 's':
		dbSaveCommand(client);
		break;
	case 'l':
		dbLoadCommand(client);
		break;
	default:
		NOT_FOUND(client);
		break;
	}
}

static void dbSaveCommand(redisClient *client)
{
	robj *o = client->argv[2];
	char *filename = (sds)o->ptr;
	FILE *fp = fopen(filename, "r");
	
	size_t pos = 0;
	char buf[BIG_BUF];
	memset(buf, 0, BIG_BUF);
	
	if(fp == NULL){
		size_t size = startCompressRDB(client->db, buf, &pos);
		fp = fopen(filename, "w");
		fwrite(buf, size, 1, fp);
		fclose(fp);
		memcpy(client->buf + client->bufpos, "(save OK)", 9);
		client->bufpos += 9;
	}
	else{
		fclose(fp);
		memcpy(client->buf + client->bufpos, "(file exists)", 13);
		client->bufpos += 13;
	}
}

static void dbLoadCommand(redisClient *client)
{
	robj *o = client->argv[2];
	char *filename = (sds)o->ptr;
	FILE *fp = fopen(filename, "r");
	size_t pos = 0;
	char buf[BIG_BUF];
	memset(buf, 0, BIG_BUF);

	if(fp != NULL){
		size_t nread = fread(buf, 1, BIG_BUF, fp);
		startLoadFromRDB(client, buf, &pos);
		fclose(fp);
		memcpy(client->buf + client->bufpos, "(load OK)", 9);
		client->bufpos += 9;
	}
	else{
		NOT_FOUND(client);
	}
}

static dict *cmdTable = initCommandTable();
void deduceCommandForQuery(redisClient *client)
{
	assert(client->argc > 0);
	memset(client->buf, 0, REDIS_QUERY_CHUNK_BYTES);
	client->bufpos = 0;
	client->cmd = NULL;

	robj *o = client->argv[0];
	char *name = (sds)o->ptr;
	dictEntry *de = dictFind(cmdTable, name);
	
	if (de != NULL) {
		redisCommand *cmd = (redisCommand *)de->val;
		client->cmd = cmd;
		if ((cmd->arbity > 0 && client->argc != cmd->arbity) ||
			(cmd->arbity < 0 && client->argc < (-cmd->arbity))) {
			memcpy(client->buf + client->bufpos, "(argc error)\n\0", 14);
			client->bufpos += 14;
			client->cmd = NULL;
			return;
		}
		((client->cmd)->proc)(client);
	}
	else {
		client->cmd = NULL;
		memcpy(client->buf + client->bufpos, "(command error)\n\0", 17);
		client->bufpos += 17;
	}
}
