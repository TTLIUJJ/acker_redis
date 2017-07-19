#pragma once

#ifndef __ZMALLOC_H_
#define __ZMALLOC_H_


void *zmalloc(size_t size);
void *zcalloc(size_t size);
void *zrealloc(void *ptr, size_t size);
size_t zmalloc_size(void *ptr);
void zfree(void *ptr);
size_t zmalloc_used_memmory();
void zmalloc_enable_thread_safeness();


#endif 