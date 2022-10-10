#include <sys/eventfd.h>

#include "dyco_coroutine.h"


int dyco_epoll_create(void) {
	return epoll_create(1024);
} 

int dyco_epoll_wait(struct timespec t) {
	dyco_schedule *sched = _get_sched();
	return epoll_wait(sched->epollfd, sched->eventlist, DYCO_MAX_EVENTS, t.tv_sec*1000.0 + t.tv_nsec/1000000.0);
}

int dyco_epoll_ev_register_trigger(void) {
	dyco_schedule *sched = _get_sched();

	if (!sched->eventfd) {
		sched->eventfd = eventfd(0, EFD_NONBLOCK);
		assert(sched->eventfd != -1);
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = sched->eventfd;
	int ret = epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, sched->eventfd, &ev);

	assert(ret != -1);
}


