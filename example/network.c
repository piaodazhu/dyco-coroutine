#include "dyco_coroutine.h"
#include <hiredis/hiredis.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <arpa/inet.h>

int inittimerfd(time_t start_ns, time_t interval_ns) {
	int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	struct itimerspec timebuf;
	timebuf.it_interval.tv_nsec = interval_ns;
	timebuf.it_interval.tv_sec = 0;
	timebuf.it_value.tv_nsec = interval_ns;
	timebuf.it_value.tv_sec = 0;
	timerfd_settime(tfd, 0, &timebuf, NULL);
	printf("inittimerfd create fd = %d\n", tfd);
	return tfd;
}

void timer(void *arg)
{
	// int tfd = inittimerfd(0, 10000); // 10ns
	int tfd = inittimerfd(0, 200000000);
	
	struct epoll_event events[3];
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
	
	dyco_epoll_init();

	ev.data.fd = tfd;
	dyco_epoll_add(ev.data.fd, &ev);

	while (1) {
		int nevents = dyco_epoll_wait(events, 3, -1);
		int ret, i = 0;
		uint64_t cnt;
		for (i = 0; i < nevents; i++) {
			ret = read(events[i].data.fd, &cnt, sizeof(cnt));
			printf(".");
		}
	}

	dyco_epoll_del(tfd, NULL);

	dyco_epoll_destroy();
	return;
}


void dns(void *arg)
{
	printf("[dns] I am coro %d.\n", dyco_coroutine_coroID());
	char *dnames[] = {"cnblogs.com", "www.baidu.com", "bilibili.com", 
			"leetcode.cn", "blog.csdn.net", "www.xuetangx.com"};
	int i = 0;
	char *dname;
	struct hostent *ret;
	while (1)
	{
		dname = dnames[i];
		ret = gethostbyname(dname);
		printf("%s: %d\n", dname, ret->h_length);
		i = (i + 1) % 6;
	}
	
	return;
}

void redis(void *arg)
{
	printf("[redis] I am coro %d.\n", dyco_coroutine_coroID());
	
	redisReply *reply;
	redisContext *c;
	c = redisConnect("127.0.0.1", 6379);
	reply = redisCommand(c, "PING");
	assert(reply != NULL);
	printf("PING: %s\n", reply->str);
	freeReplyObject(reply);

	while (1)
	{
		// set
		char *set_ = "set Key Value";
		reply = redisCommand(c, set_);
		if (reply != NULL && reply->type == REDIS_REPLY_STATUS)
		{
			printf("%s\n", reply->str);
		}
		freeReplyObject(reply);

		// get
		char *get_ = "get Key";
		reply = redisCommand(c, get_);
		if (reply != NULL && reply->type == REDIS_REPLY_STRING)
		{
			printf("%s\n", reply->str);
		}
		freeReplyObject(reply);

		// del
		char *del_ = "del Key";
		reply = redisCommand(c, del_);
		if (reply != NULL && reply->type == REDIS_REPLY_STRING)
		{
			printf("%s\n", reply->str);
		}
		freeReplyObject(reply);

		// dyco_coroutine_sleep(2000);
	}
	return;
}

void server_reader(void *arg) {
	int fd = *(int *)arg;
	int ret = 0;

	while (1) {
		
		char buf[1024] = {0};
		ret = recv(fd, buf, 1024, 0);
		if (ret > 0) {
			printf("From Client: %.*s\n", ret, buf);

			ret = send(fd, buf, strlen(buf), 0);
			if (ret == -1) {
				dyco_close(fd);
				printf("server_reader send failed: fd=%d\n",fd);
				break;
			}
		} else if (ret == 0) {	
			printf("Shutdown\n");
			close(fd);
			break;
		}

	}
	return;
}

void tcpserver(void *arg)
{
	printf("[tcpserver] I am coro %d.\n", dyco_coroutine_coroID());
	
	unsigned short port = 9096;
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in local, remote;
	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	local.sin_addr.s_addr = INADDR_ANY;
	bind(fd, (struct sockaddr*)&local, sizeof(struct sockaddr_in));

	listen(fd, 20);
	printf("listen port : %d\n", port);

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

	while (1) {
		socklen_t len = sizeof(struct sockaddr_in);
		int cli_fd = accept(fd, (struct sockaddr*)&remote, &len);
		printf("new client comming\n");
		dyco_coroutine_create(server_reader, &cli_fd);
	}

	return;
}

int main()
{
	dyco_coroutine_create(timer, NULL);
	dyco_coroutine_create(dns, NULL);
	dyco_coroutine_create(redis, NULL);
	dyco_coroutine_create(tcpserver, NULL);

	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");
	return 0;
}