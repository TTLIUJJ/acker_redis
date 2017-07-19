#pragma once
#ifndef __ADLIST_H_
#define __ADLIST_H_



#define AL_START_HEAD 0
#define AL_START_TAIL 1

typedef struct listNode {
	struct listNode *prev;
	struct listNode *next;
	void *value;
}listNode;

typedef struct list {
	listNode *head;
	listNode *tail;
	void *(*dup)(void *ptr);
	void (*free)(void *ptr);
	int (*match)(void *ptr1, void *ptr2);
	unsigned long len;
}list;

typedef struct listIterator {
	listNode *next;
	int direction;
}listIterator;

#define listLength(l) ((l)->len)
#define listFirst(l) ((l)->head)
#define listTail(l) ((l)->tail)
#define listPrevNode(n) ((n)->prev)
#define listNextNode(n) ((n)->next)
#define listNodeValue(n) ((n)->value)

#define listGetDupMethod(l) ((l)->dup)
#define lsitGetFreeMethod(l) ((l)->free)
#define listGetMatchMethod(l) ((l)->match)

#define listSetDupMethod(l, m) ((l)->dup = (m))
#define listSetFreeMethod(l, m) ((l)->free = (m))
#define listSetMatchMethod(l, m) ((l)->match = (m))

list *listCreate();
list *listAddNodeHead(list *l, void *value);
list *listAddNodeTail(list *l, void *value);
list *listInsertNode(list *l, listNode *old_node, void *value, int after);
listIterator *listGetIterator(list *l, int direction);
void listReleaseIterator(listIterator *iter);
listNode *listNext(listIterator *iter);
listNode *listSearchKey(list *l, void *key);
void listDelNode(list *l, listNode *node);
void listRelease(list *l);
int listmatchsds(void *key1, void *key2);


#endif 