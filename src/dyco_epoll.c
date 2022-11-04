#include "dyco_coroutine.h"

int
dyco_epoll_init()
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		perror("ERROR: dyco_epoll_* can only be called inside coroutine functions!");
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		perror("ERROR: dyco_epoll_* can only be called inside coroutine functions!");
		return -1;
	}
	
	if (TESTBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING)) {
		perror("ERROR: dyco_epoll_init can only be called once in one coroutine function! You can use normal epollfd and add it by dyco_epoll_ctl");
		return -1;
	}
	co->epollfd = epoll_create(1024);
	SETBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING);

	return 0;
} 

int
dyco_epoll_wait(struct epoll_event *__events, int __maxevents, int __timeout)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		perror("ERROR: dyco_epoll_* can only be called inside coroutine functions!");
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		perror("ERROR: dyco_epoll_* can only be called inside coroutine functions!");
		return -1;
	}
	if (!TESTBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING)) {
		perror("ERROR: 4dyco_epoll_init haven't been called!");
		ABORT();
		return -1;
	}

	// fast return
	if (__timeout == 0) {
		return epoll_wait_f(co->epollfd, __events, __maxevents, 0);
	}

	struct epoll_event ev;
	ev.data.fd = co->epollfd;
	ev.events = EPOLLIN | EPOLLHUP | EPOLLERR;
	epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, co->epollfd, &ev);
	_schedule_sched_wait(co, co->epollfd);

	_schedule_sched_sleep(co, __timeout);

	_yield(co);

	_schedule_cancel_sleep(co);

	_schedule_cancel_wait(co, co->epollfd);
	epoll_ctl(sched->epollfd, EPOLL_CTL_DEL, co->epollfd, NULL);

	return epoll_wait_f(co->epollfd, __events, __maxevents, 0);
}

int
dyco_epoll_add(int __fd, struct epoll_event *__ev)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		perror("ERROR: dyco_epoll_* can only be called inside coroutine functions!");
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		perror("ERROR: dyco_epoll_* can only be called inside coroutine functions!");
		return -1;
	}
	if (!TESTBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING)) {
		perror("ERROR: 3dyco_epoll_init haven't been called!");
		return -1;
	}
	if (epoll_ctl(co->epollfd, EPOLL_CTL_ADD, __fd, __ev) < 0) {
		perror("ERROR: could not add event to dyco_epoll");
		return -1;
	}
	return 0;
}

int
dyco_epoll_del(int __fd, struct epoll_event *__ev)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		perror("ERROR: dyco_epoll_* can only be called inside coroutine functions!");
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		perror("ERROR: dyco_epoll_* can only be called inside coroutine functions!");
		return -1;
	}
	if (!TESTBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING)) {
		perror("ERROR: 2dyco_epoll_init haven't been called!");
		return -1;
	}
	if (epoll_ctl(co->epollfd, EPOLL_CTL_DEL, __fd, __ev) < 0) {
		perror("ERROR: could not delete event from dyco_epoll");
		return -1;
	}
	return 0;
}

void
dyco_epoll_destroy()
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		perror("ERROR: dyco_epoll_* can only be called inside coroutine functions!");
		return;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		perror("ERROR: dyco_epoll_* can only be called inside coroutine functions!");
		return;
	}
	if (!TESTBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING)) {
		perror("ERROR: 1dyco_epoll_init haven't been called!");
		return;
	}

	
	CLRBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING);
	close(co->epollfd);
	
	return;
}

#ifdef COROUTINE_HOOK

int
epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	if (timeout == 0) {
		return epoll_wait_f(epfd, events, timeout, 0);
	}

	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return -1;
	}

	struct epoll_event ev;
	ev.data.fd = epfd;
	ev.events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET;
	epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, epfd, &ev);
	_schedule_sched_wait(co, epfd);
	_schedule_sched_sleep(co, timeout);

	_yield(co);

	_schedule_cancel_sleep(co);
	_schedule_cancel_wait(co, epfd);
	epoll_ctl(sched->epollfd, EPOLL_CTL_DEL, epfd, NULL);

	return epoll_wait_f(epfd, events, maxevents, 0);
}

int 
poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	if (timeout == 0) {
		return poll_f(fds, nfds, 0);
	}

	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return -1;
	}

	struct epoll_event ev;

	int epfd = epoll_create(nfds);
	int i, ret;
	for (i = 0; i < nfds; i++) {
		ev.events = fds[i].events;
		ev.data.fd = fds[i].fd;
		ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fds[i].fd, &ev);
		assert(ret == 0);
	}
	
	ev.data.fd = epfd;
	ev.events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET;
	epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, epfd, &ev);
	_schedule_sched_wait(co, epfd);
	_schedule_sched_sleep(co, timeout);

	_yield(co);

	_schedule_cancel_sleep(co);
	_schedule_cancel_wait(co, epfd);
	epoll_ctl(sched->epollfd, EPOLL_CTL_DEL, epfd, NULL);

	for (i = 0; i < nfds; i++) {
		ret = epoll_ctl(epfd, EPOLL_CTL_DEL, fds[i].fd, NULL);
		assert(ret == 0);
	}

	return poll_f(fds, nfds, timeout);
}

#endif
