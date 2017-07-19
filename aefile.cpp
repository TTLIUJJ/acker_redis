#include "redis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>

void append_event(int epollfd, int fd, int state);
void modify_event(int epollfd, int fd, int state);
void delete_event(int epollfd, int fd, int state);

aeEventLoop *aeCreateFileEvent(int epollfd, int fd, int mask)
{
	aeEventLoop *el = (aeEventLoop *)zmalloc(sizeof(struct aeEventLoop));
	el->rproc = NULL;
	el->wproc = NULL;
	el->epollfd = epollfd;

	if (fd > el->maxfd)
		el->maxfd = fd;
	el->fd = fd;
	el->mask = mask;
	
	return el;
}

//process command form internet 
//svae the read data in redisClient.querybuf.
void fileProcFromNet(struct aeEventLoop *el, redisClient *client)
{
	if (!(el->mask & REDIS_READABLE))
		assert(0);
	client->fd = el->fd;
	client->querybuf = (char *)malloc(REDIS_QUERY_CHUNK_BYTES * sizeof(char));
	memset(client->querybuf, 0, REDIS_QUERY_CHUNK_BYTES);
	char query[REDIS_QUERY_CHUNK_BYTES];
	size_t totlen = REDIS_QUERY_CHUNK_BYTES;
	size_t nread = 0;
	
	if((nread = read(el->fd, client->querybuf, REDIS_QUERY_CHUNK_BYTES)) == -1){
		perror("read error:");
		close(el->fd);
		delete_event(el->epollfd, el->fd, EPOLLIN);
	}
	else if(nread == 0){
		fprintf(stderr, "client close.\n");
		close(el->fd);
		delete_event(el->epollfd, el->fd, EPOLLIN);
	}
	else{
		memset(client->querybuf + nread, 0, 1);
		parseQuery(client);
		deduceCommandForQuery(client);
		modify_event(el->epollfd, el->fd, EPOLLOUT);
		free(client->querybuf);
		client->querybuf = NULL;
		el->rproc = NULL;
	}
}

//send the processed data for the client.
//and release some memmory or some reseted operations.
void fileProcIntoNet(struct aeEventLoop *el, redisClient *client)
{
	if (!(el->mask & 2))
		assert(0);
		
	client->fd = el->fd;
	size_t nwrite = 0;

	memcpy(client->buf+client->bufpos, "\n", 1);
	++client->bufpos;
	if((nwrite = write(el->fd, client->buf, client->bufpos)) == -1){
		perror("write error: ");
		close(el->fd);
		delete_event(el->epollfd, el->fd, EPOLLOUT);
	}
	else{
		modify_event(el->epollfd, el->fd, EPOLLIN);
		memset(client->buf, 0, client->bufpos);
		client->bufpos = 0;
		el->wproc = NULL;
	}
}

void append_event(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

void modify_event(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

void delete_event(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
}
