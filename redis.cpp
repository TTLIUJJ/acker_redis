#include "redis.h"


// ========================  Q  U  E  R  Y  ==============================
static robj* parseQueryObject(char *buf, size_t *pos, size_t slen);

//before call parseQuery, it must be set memory for client->argv
void parseQuery(redisClient *client)
{
	size_t pos = 0;
	int argc = 0;
	int i = 0;
	size_t diff;
	size_t buflen = strlen(client->querybuf);
	//as far as I known, it will receive the 127 ASCII at first correspondence, so the 10 and ASCII must be deleted 	
	//and the last correspondence we just exclude the 10 ASCII
	//and it may be includes other situations beyond i known.
	//so in the loop more charcters are comsidered.
	char *p = client->querybuf + buflen;
	for(i = 1; i < 10; ++i){
		--p;
		if(*p == '\n')
			break;
	}
	buflen -= i;
	memset(client->querybuf + buflen, 0, 1);
	while (pos < buflen) {
		char *p = strchr(client->querybuf + pos, ' ');
		robj *o;
	
		if (p == NULL) {
			diff = buflen - pos;
			o = parseQueryObject(client->querybuf, &pos, diff);
		}
		else {
			diff = p - (client->querybuf + pos);
			if (diff == 0) {
				++pos;
				continue;
			}
			o = parseQueryObject(client->querybuf, &pos, diff);
			//for skip ' '
			++pos;
		}
		client->argv[argc] = o;
		++argc;
	}
	client->argc = argc;
}

static robj* parseQueryObject(char *buf, size_t *pos, size_t slen)
{
	char query[MAXQUERY];
	memcpy(query, buf + *pos, slen);
	query[slen] = '\0';
	*pos += slen;
	robj *o = createStringObject(query);

	return o;
}

redisClient *initRedisClient()
{
	redisClient *client = (redisClient *)zmalloc(sizeof(struct redisClient));
	redisCommand *cmd = (redisCommand *)zmalloc(sizeof(struct redisCommand));
	redisDb *db = (redisDb *)zmalloc(sizeof(struct redisDb));
	dict *d = dictCreate(&dictObjectType, NULL);
	db->dict = d;
	client->db = db;
	
	client->fd = -1;
	client->argv = (robj **)zmalloc(MAXQUERYARGC * sizeof(robj *));
	client->argc = 0;
	client->cmd = NULL;
	client->querybuf = NULL;
	client->bufpos = 0;
	memset(client->buf, 0, REDIS_REPLY_CHUNK_BYTES);

	return client;
}


/*


#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>


#define IPADDRESS "127.0.01"
#define PORT 54321
#define MAXSIZE 1024
#define LISTENQ 10
#define FDSIZE 1000
#define EPOLLEVENTS 100

static int socketBind(const char *ip, int port)
{
	int listenfd;
	struct sockaddr_in servaddr;
	
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd == -1) {
		perror("socket error: ");
		exit(1);
	}
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &servaddr.sin_addr);

	if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
		perror("bind error: ");
		exit(1);
	}

	return listenfd;
}

static void hardToGiveName(int listenfd)
{

	int ret;
	char buf[MAXSIZE];
	memset(buf, 0, MAXSIZE);
	int epollfd;
	struct epoll_events events[EPOLLEVENTS];

	epollfd = epoll_create(FDSIZE);
	addListenEvent(epollfd, listenfd, EPOLLIN);
	for (; ; ) {
		ret = epoll_wait(epollfd, events, EPOLLEVENTS, -1);
		
	}

	close(epollfd);
}

static void addListenEvent(int epollfd, int listenfd, int type)
{
	
}


void setUp()
{
	int listenfd;
	listenfd = socketBind(IPADDRESS, PORT);
	listen(listenfd, LISTENQ);
	hardToGiveName(listenfd);
	//release some globar variable that pointer to memeroy.
}

*/

