#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "sds.h"
#include "zmalloc.h"

sds sdsNewlen(const void *init, size_t len)
{
	struct sdshdr *sh;

	if (init) {
		sh = (struct sdshdr *)zmalloc(sizeof(struct sdshdr) + len + 1);
	}
	else {
		sh = (struct sdshdr *)zcalloc(sizeof(struct sdshdr) + len + 1);
	}
	if (sh == NULL)
		return NULL;
	sh->len = len;
	sh->free = 0;
	if (init && len)
		memcpy(sh->buf, init, len);
	sh->buf[len] = '\0';

	return (char *)sh->buf;
}

sds sdsEmpty()
{
	return sdsNewlen("", 0);
}

sds sdsNew(const char *init)
{
	size_t len = (init == NULL) ? 0 : strlen(init);
	return sdsNewlen(init, len);
}

sds sdsUp(const sds s)
{
	return sdsNewlen(s, strlen(s));
}

void sdsFree(sds s)
{
	if (s == NULL)
		return;
	char *p = s - sizeof(struct sdshdr);
	zfree(p);
}

void sdsClear(sds s)
{
	struct sdshdr *sh = (struct sdshdr *)(s - sizeof(struct sdshdr));
	sh->free += sh->len;
	sh->len = 0;
	sh->buf[0] = '\0';
}

int sdsCmp(const sds s1, const sds s2)
{
	size_t l1, l2, minlen;
	
	l1 = strlen(s1);
	l2 = strlen(s2);
	minlen = l1 > l2 ? l2 : l1;
	int cmp = memcmp(s1, s2, minlen);
	if (cmp == 0)
		return l1 - l2;
	else
		return cmp;
}

sds sdsMakeRoomFor(sds s, size_t addlen)
{
	struct sdshdr *sh, *newsh;
	size_t free = sdsavail(s);
	size_t len, newlen;

	if (free > addlen)
		return s;
	
	sh = (struct sdshdr *)(s - sizeof(struct sdshdr));
	len = sdslen(s);
	newlen = sh->len + addlen;
	if (newlen < SDS_MAX_PERALLOC)
		newlen *= 2;
	else
		newlen += SDS_MAX_PERALLOC;
	
	newsh = (struct sdshdr *)zrealloc(sh, sizeof(struct sdshdr) + newlen + 1);
	if (newsh == NULL)
		return NULL;

	newsh->len = newlen;
	newsh->free = newlen - len;

	return newsh->buf;
}

sds sdsCat(sds s, const char *p)
{
	size_t curlen = sdslen(s);
	size_t len = strlen(p);
	
	s = sdsMakeRoomFor(s, len);
	if (s == NULL)
		return NULL;

	struct sdshdr *sh = (struct sdshdr *)(s - (sizeof(struct sdshdr)));
	memcpy(s + curlen, p, len);

	sh->len = curlen + len;
	sh->free = sh->free - len;
	s[curlen + len] = '\0';

	return s;
}