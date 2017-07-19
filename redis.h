#pragma once
#ifndef __REDIS_H_
#define __REDIS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <inttypes.h>
#include <assert.h>

#include "adlist.h"
#include "dict.h"
#include "sds.h"
#include "zmalloc.h"

#define REDIS_OK   0
#define REDIS_ERR -1

#define REDIS_STRING 1
#define REDIS_LIST   2
#define REDIS_HASH   3

#define MASSIVE (1024 * 1024 * 1024)
#define MAXQUERY 1024
#define MAXQUERYARGC 128

#define REDIS_RDB_ISNUMBER(buf) (*(buf) <= '9' && *(buf) >= '1')

#define REDIS_READABLE (1)
#define REDIS_WRITABLE (1 << 1)
#define REDIS_RERWABLE (1 << 2)

extern struct redisServer server;
extern dictType dictObjectType;


typedef struct redisObject {
	unsigned int type;
	int refcount;
	void *ptr;
}robj;

typedef struct redisDb {
	struct dict *dict;
	int id;			//数据库编号,目前只考虑1个
}redisDb;


#define REDIS_REPLY_CHUNK_BYTES (1024 * 16)
#define REDIS_QUERY_CHUNK_BYTES 1024
typedef struct redisClient {
	redisDb *db;		//客户正引用的数据库
	int fd;				//客户的套接字描述符
	char *querybuf;	//输入缓冲区，保留客户向服务器的请求
	int argc;
	robj **argv;			//分析querybuf,依次填充为argv[0], argv[1]....
						//每个都是字符串对象
	struct redisCommand *cmd;	//实现请求的调用的命令
	
	int bufpos;							//buf缓存请求命令的结果
	char buf[REDIS_REPLY_CHUNK_BYTES];	
}redisClient;

struct aeEventLoop;

typedef void aeFileProcWrite(struct aeEventLoop *el, redisClient *client);
typedef void aeFileProcRead(struct aeEventLoop *el, redisClient *client);

typedef struct aeEventLoop {
	int maxfd;
	int mask;			//READABLE | WRITEABLE
	int fd;
	int epollfd;
	aeFileProcRead *rproc;
	aeFileProcWrite *wproc;
}aeEventLoop;

//只有一个服务器，并且是全局变量
struct redisServer {
	redisDb *db;		//不同于客户端，服务的db是数据库的数组
	int dbnum;
	list *clients;		//保存客户端状态的链表
	aeEventLoop *el;	//事件循环
	int port;			//端口号
};

typedef void redisCommandProc(redisClient *c);
typedef struct redisCommand {
	char *name;
	int arbity;
	redisCommandProc *proc;
}redisCommand;


//API of Redis
void parseQuery(redisClient *client);
redisClient *initRedisClient();

//API of Command
dict *initCommandTable();
void getCommand(redisClient *client);
void setCommand(redisClient *client);
void delCommand(redisClient *client);
void lpushCommand(redisClient *client);
void lpopCommand(redisClient *client);
void lindexCommand(redisClient *client);
void hgetCommand(redisClient *client);
void hsetCommand(redisClient *client);
void hlenCommand(redisClient *client);
void typeCommand(redisClient *client);
void dbCommand(redisClient *client);
void deduceCommandForQuery(redisClient *client);

//API of Object
robj *createObject(int type, void *ptr);
robj *createStringObject(char *ptr);
robj *createListObject();
robj *createHashObject();
void freeStringObject(void *o);
void freeListObject(void *o);
void freeHashObject(void *o);
void decrRefCount(robj *o);
void incrRefCount(robj *o);
int keyStringObjectCompare(void *privdata, const void *key1, const void *key2);
void decrRefCountVoid(void *o);
int keyStringObjectCompare(void *privdata, const void *key1, const void *key2);
unsigned int dictStringObjectHashFuction(const void *key);
void stringObjectDestructor(void *privdata, void *key);
void *stringObjectDup(void *pirvdata, const void *key);

//API of Rdb
size_t compressToRDB(dictEntry *de, char *buf, size_t *pos);
size_t compressSdsToRDB(sds s, char *buf, size_t *pos);
size_t compressKeyToRDB(robj *key, char *buf, size_t *pos, int type);
size_t compressStringValueToRDB(robj *val, char *buf, size_t *pos);
size_t compressListValueToRDB(robj *val, char *buf, size_t *pos);
size_t compressHashValueToRDB(robj *val, char *buf, size_t *pos);
size_t startCompressRDB(redisDb *db, char *buf, size_t *pos);
void startLoadFromRDB(redisClient *client, char *buf, size_t *pos);
void extractFromRDB(dict *d, char *buf, size_t *pos);
size_t extractFromSdsLen(char *buf, size_t *pos);
sds extractSdsFromRDB(char *buf, size_t *pos);
robj *extractStringObjectFromRDB(char *buf, size_t *pos);
robj *extractListObjectFromRDB(char *buf, size_t *pos);
robj *extractHashObjectFromRDB(char *buf, size_t *pos);

//API of Db
int dbModifyStringVal(redisDb *db, void *key, void *val);
void dbRemoveHashEntry(redisDb *db, void *key, void *val);
void removeHashEntry(dict *d, void *key, void *val);

//API of Aefile
aeEventLoop *aeCreateFileEvent(int epollfd, int fd, int mask);
void fileProcIntoNet(struct aeEventLoop *el, redisClient *client);
void fileProcFromNet(struct aeEventLoop *el, redisClient *client);
void append_event(int epollfd, int fd, int state);
void modify_event(int epollfd, int fd, int state);
void delete_event(int epollfd, int fd, int state);


#endif	//__REDIS_H_
