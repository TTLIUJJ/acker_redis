#include <stdio.h>
#include <string.h>

#include "zmalloc.h"
#include "adlist.h"

list *listCreate()
{
	list *l = (list *)zmalloc(sizeof(struct list));
	
	if (l == NULL)
		return NULL;
	l->head = NULL;
	l->tail = NULL;
	l->len = 0;
	l->dup = NULL;
	l->free = NULL;
	l->match = NULL;

	return l;
}

list *listAddNodeHead(list *l, void *value)
{
	listNode *node = (listNode *)zmalloc(sizeof(struct listNode));
	
	if (node == NULL)
		return NULL;

	node->value = value;

	if (l->len != 0) {
		node->prev = NULL;
		node->next = l->head;
		l->head->prev = node;
		l->head = node;
	}
	else {
		l->head = l->tail = node;
		node->prev = NULL;
		node->next = NULL;
	}
	++l->len;

	return l;
}

list *listAddNodeTail(list *l, void *value)
{
	listNode *node = (listNode *)zmalloc(sizeof(struct listNode));

	if (node == NULL)
		return NULL;
	node->value = value;
	if(l->len != 0){
		l->tail->next = node;
		node->prev = l->tail;
		node->next = NULL;
		l->tail = node;
	}
	else {
		l->head = node;
		l->tail = node;
	}
	++l->len;

	return l;
}

list *listInsertNode(list *l, listNode *old_node, void *value, int after)
{
	listNode *node = (listNode *)zmalloc(sizeof(struct listNode));

	if (node == NULL)
		return NULL;

	node->value = value;
	if (after) {
		node->prev = old_node;
		node->next = old_node->next;
		if (old_node == l->tail)
			l->tail = node;
	}
	else {
		node->prev = old_node->prev;
		node->next = old_node;
		if (old_node == l->head)
			l->head = node;
	}
	if (node->prev != NULL)
		old_node->prev->next = node;
	if (node->next != NULL)
		old_node->next->prev = node;
	++l->len;

	return l;
}

//不需要迭代器的时候要释放内存
//listReleaseIterator
listIterator *listGetIterator(list *l, int direction)
{
	listIterator *iter = (listIterator *)zmalloc(sizeof(struct listIterator));
	
	if (iter == NULL)
		return NULL;
	iter->direction = direction;
	
	if (direction == AL_START_HEAD)
		iter->next = l->head;
	else 
		iter->next = l->tail;

	return iter;
}

void listReleaseIterator(listIterator *iter)
{
	zfree(iter);
}

listNode *listNext(listIterator *iter)
{
	listNode *cur = iter->next;
	
	if (cur != NULL) {
		if (iter->direction == AL_START_HEAD)
			iter->next = cur->next;
		else
			iter->next = cur->prev;
	}

	return cur;
}

listNode *listSearchKey(list *l, void *key)
{
	listIterator *iter = listGetIterator(l, AL_START_HEAD);
	listNode *node;

	while ((node = listNext(iter)) != NULL) {
		if (l->match) {
			if (l->match(node->value, key)) {
				listReleaseIterator(iter);
				return node;
			}
		}
		else {
			if (node->value == key) {
				listReleaseIterator(iter);
				return node;
			}
		}
	}
	listReleaseIterator(iter);

	return NULL;
}

void listDelNode(list *l, listNode *node)
{
	if (node->prev)
		node->prev->next = node->next;
	else
		l->head = node->next;

	if (node->next)
		node->next->prev = node->prev;
	else
		l->tail = node->prev;

	--l->len;

	zfree(node);
}

void listRelease(list *l)
{
	listNode *next, *cur;
	unsigned long len;

	if (l == NULL)
		return;
	cur = l->head;
	len = l->len;
	while (len--) {
		next = cur->next;
		if (l->free)
			l->free(cur->value);
		zfree(cur);
		cur = next;
	}
	zfree(l);
}

int listmatchsds(void *key1, void *key2)
{
	int cmp;
	char *k1 = (char *)key1;
	char *k2 = (char *)key2;

	int len1 = strlen(k1);
	int len2 = strlen(k2);
	if (len1 != len2)
		return 0;

	cmp = memcmp(k1, k2, len1);
	if (cmp == 0)
		return 1;
	else
		return 0;
}