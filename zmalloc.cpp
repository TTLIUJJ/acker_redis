
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zmalloc.h"

#define PREFIX_SIZE (sizeof(size_t))

static size_t used_memory = 0;
static int zmalloc_thread_safe = 0;

#define update_zmalloc_stat_add(__n)\
		do{\
			used_memory += (__n);\
		}while(0)

#define update_zmalloc_stat_sub(__n)\
		do{\
			used_memory -= (__n);\
		}while(0)

#define update_zmalloc_stat_alloc(__n)\
		do{\
			size_t _n = (__n);\
			if(_n &(sizeof(long) -1))\
				_n += sizeof(long) - (_n & (sizeof(long)-1));\
			if(zmalloc_thread_safe){\
				update_zmalloc_stat_add(_n);\
			}\
			else{\
				used_memory += _n;\
			}\
		}while(0)

#define update_zmalloc_stat_free(__n)\
		do{\
			size_t _n = (__n);\
			if(_n &(sizeof(long) -1))\
				_n += sizeof(long) - (_n & (sizeof(long)-1));\
			if(zmalloc_thread_safe){\
				update_zmalloc_stat_sub(_n);\
			}\
			else{\
				used_memory -= _n;\
			}\
		}while(0)

static void zmalloc_oom(size_t size)
{
	fprintf(stderr, "zmalooc: Out of Memory tring to alloac %zu bytes\n", size);
	fflush(stderr);
	abort();
}

void *zmalloc(size_t size)
{
	void *ptr = malloc(size + PREFIX_SIZE);
	if (!ptr)
		zmalloc_oom(size);
	*((size_t *)ptr) = size;
	update_zmalloc_stat_alloc(size + PREFIX_SIZE);

	return (char *)ptr + PREFIX_SIZE;
}

void *zcalloc(size_t size)
{
	void *ptr = calloc(1, size + PREFIX_SIZE);
	if (!ptr)
		zmalloc_oom(size);
	*((size_t *)ptr) = size;
	update_zmalloc_stat_add(size + PREFIX_SIZE);
	
	return (char *)ptr + PREFIX_SIZE;
}

void *zrealloc(void *ptr, size_t size)
{
	void *realptr, *newptr;
	size_t old_size;

	if (ptr == NULL)
		return zmalloc(size);

	realptr = (char*)ptr - PREFIX_SIZE;
	old_size = *((size_t*)realptr);
	newptr = realloc(realptr, size + PREFIX_SIZE);
	if (!newptr)
		zmalloc_oom(size);

	*((size_t*)newptr) = size;
	update_zmalloc_stat_free(old_size);
	update_zmalloc_stat_alloc(size);

	return (char*)newptr + PREFIX_SIZE;
}

size_t zmalloc_size(void *ptr)
{
	void *realptr = (char *)ptr - PREFIX_SIZE;
	size_t size = *((size_t *)realptr);
	if (size & (sizeof(long) - 1))
		size += sizeof(long) - (size&(sizeof(long) - 1));

	return size + PREFIX_SIZE;
}

void zfree(void *ptr)
{
	void *realptr;
	size_t old_size;

	if (ptr == NULL)
		return;
	realptr = (char *)ptr - PREFIX_SIZE;
	old_size = *((size_t *)ptr);
	update_zmalloc_stat_free(old_size + PREFIX_SIZE);
	free(realptr);
}

size_t zmalloc_used_memmory()
{
	size_t um;
	if (zmalloc_thread_safe) {
		um = used_memory;
	}
	else
		um = used_memory;

	return um;
}

void zmalloc_enable_thread_safeness()
{
	zmalloc_thread_safe = 1;
}

