#pragma once
#ifndef __SDS_H_
#define __SDS_H_
#include <sys/types.h>
#include <stdarg.h>

#define SDS_MAX_PERALLOC (1024 * 1024)
typedef char * sds;

struct sdshdr {
	unsigned int len;
	unsigned int free;
	char buf[];
};

static inline
size_t sdslen(const sds s)
{
	struct sdshdr *sh = (struct sdshdr *)(s - sizeof(struct sdshdr));
	return sh->len;
}

static inline 
size_t sdsavail(const sds s)
{
	struct sdshdr *sh = (struct sdshdr *)(s - sizeof(struct sdshdr));
	return sh->free;
}

sds sdsNewlen(const void *init, size_t len);
sds sdsEmpty();
sds sdsNew(const char *init);
sds sdsUp(const sds s);
void sdsFree(sds s);
void sdsClear(sds s);
int sdsCmp(const sds s1, const sds s2);
sds sdsMakeRoomFor(sds s, size_t addlen);
sds sdsCat(sds s, const char *p);




#endif 