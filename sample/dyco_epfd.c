#include "dyco_coroutine.h"
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/timerfd.h>

int inittimerfd(int start_s, int interval_s) {
	int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	struct itimerspec timebuf;
	timebuf.it_interval.tv_nsec = 0;
	timebuf.it_interval.tv_sec = interval_s;
	timebuf.it_value.tv_nsec = 0;
	timebuf.it_value.tv_sec = start_s;
	timerfd_settime(tfd, 0, &timebuf, NULL);
	printf("inittimerfd create fd = %d\n", tfd);
	return tfd;
}

void cofunc1(void *arg)
{
	int id = *(int*)arg;
	int tfd1 = inittimerfd(5, 1);
	int tfd2 = inittimerfd(6, 1);
	int tfd3 = inittimerfd(7, 2);
	
	struct epoll_event events[3];
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
	
	int epollfd = epoll_create(1);

	ev.data.fd = tfd1;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, tfd1, &ev);
	ev.data.fd = tfd2;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, tfd2, &ev);
	ev.data.fd = tfd3;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, tfd3, &ev);
	int x = 0;
	while (x < 16) {
		int nevents = epoll_wait(epollfd, events, 3, -1);
		int ret, i = 0;
		uint64_t cnt;
		for (i = 0; i < nevents; i++) {
			ret = read(events[i].data.fd, &cnt, sizeof(cnt));
			printf("[%ld] cofunc1: fd %d, read counter val %lu\n", time(NULL), events[i].data.fd, cnt);
			++x;
		}
	}
	close(tfd1);
	close(tfd2);
	close(tfd3);
	close(epollfd);
	return;
}

// void cofunc2(void *arg)
// {
// 	int id = *(int*)arg;
// 	int tfd1 = inittimerfd(1, 1);
// 	int tfd2 = inittimerfd(2, 1);
// 	int tfd3 = inittimerfd(1, 2);
	
// 	struct epoll_event events[3];
// 	struct epoll_event ev;
// 	ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
	
// 	int epollfd = epoll_create(1);

// 	ev.data.fd = tfd1;
// 	epoll_ctl(epollfd, EPOLL_CTL_ADD, tfd1, &ev);
// 	ev.data.fd = tfd2;
// 	epoll_ctl(epollfd, EPOLL_CTL_ADD, tfd2, &ev);
// 	ev.data.fd = tfd3;
// 	epoll_ctl(epollfd, EPOLL_CTL_ADD, tfd3, &ev);
// 	int x = 0;
// 	while (x < 16) {
// 		int nevents = epoll_wait(epollfd, events, 3, -1);
// 		int ret, i = 0;
// 		uint64_t cnt;
// 		for (i = 0; i < nevents; i++) {
// 			ret = read(events[i].data.fd, &cnt, sizeof(cnt));
// 			printf("[%ld] cofunc2: fd %d, read counter val %lu\n", time(NULL), events[i].data.fd, cnt);
// 			++x;
// 		}
// 	}
// 	close(tfd1);
// 	close(tfd2);
// 	close(tfd3);
// 	close(epollfd);
// 	return;
// }

// void cofunc1(void *arg)
// {
// 	int id = *(int*)arg;
// 	int tfd1 = inittimerfd(5, 1);
// 	int tfd2 = inittimerfd(6, 1);
// 	int tfd3 = inittimerfd(7, 2);
	
// 	struct epoll_event events[3];
// 	struct epoll_event ev;
// 	ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
	
// 	dyco_epoll_init();

// 	ev.data.fd = tfd1;
// 	dyco_epoll_add(ev.data.fd, &ev);
// 	ev.data.fd = tfd2;
// 	dyco_epoll_add(ev.data.fd, &ev);
// 	ev.data.fd = tfd3;
// 	dyco_epoll_add(ev.data.fd, &ev);
// 	int x = 0;
// 	while (x < 16) {
// 		int nevents = dyco_epoll_wait(events, 3, -1);
// 		int ret, i = 0;
// 		uint64_t cnt;
// 		for (i = 0; i < nevents; i++) {
// 			ret = read(events[i].data.fd, &cnt, sizeof(cnt));
// 			printf("[%ld] cofunc1: fd %d, read counter val %lu\n", time(NULL), events[i].data.fd, cnt);
// 			++x;
// 		}
// 	}

// 	// dyco_epoll_destroy();
// 	return;
// }

void cofunc2(void *arg)
{
	int id = *(int*)arg;
	int tfd1 = inittimerfd(1, 1);
	int tfd2 = inittimerfd(2, 1);
	int tfd3 = inittimerfd(1, 2);
	
	struct epoll_event events[3];
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
	
	dyco_epoll_init();

	ev.data.fd = tfd1;
	dyco_epoll_add(ev.data.fd, &ev);
	ev.data.fd = tfd2;
	dyco_epoll_add(ev.data.fd, &ev);
	ev.data.fd = tfd3;
	dyco_epoll_add(ev.data.fd, &ev);

	int x = 0;
	while (x < 16) {
		int nevents = dyco_epoll_wait(events, 3, -1);
		int ret, i = 0;
		uint64_t cnt;
		for (i = 0; i < nevents; i++) {
			ret = read(events[i].data.fd, &cnt, sizeof(cnt));
			printf("[%ld] cofunc1: fd %d, read counter val %lu\n", time(NULL), events[i].data.fd, cnt);
			++x;
		}
	}

	dyco_epoll_del(tfd1, NULL);
	dyco_epoll_del(tfd2, NULL);
	dyco_epoll_del(tfd3, NULL);

	dyco_epoll_destroy();
	return;
}

int main()
{
	// init_hook();
	dyco_coroutine *co1, *co2;
	int id1 = 1, id2 = 2;
	dyco_coroutine_create(&co1, cofunc1, &id1);
	dyco_coroutine_create(&co2, cofunc2, &id2);
	dyco_schedule_run();
	return 0;
}