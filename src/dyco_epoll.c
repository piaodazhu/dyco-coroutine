#include "dyco/dyco_coroutine.h"

int
dyco_epoll_init()
{
	dyco_schedule *sched = get_sched();
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
	DYCO_MUSTNOT(co->epollfd == -1);
	SETBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING);

	return 0;
} 

int
dyco_epoll_wait(struct epoll_event *events, int maxevents, int timeout)
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		perror("ERROR: dyco_epoll_* can only be called inside coroutine functions!");
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		perror("ERROR: dyco_epoll_* can only be called inside coroutine functions!");
		return -1;
	}
	assert(!TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));
	if (!TESTBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING)) {
		perror("ERROR: 4dyco_epoll_init haven't been called!");
		DYCO_ABORT();
		return -1;
	}

	// fast return
	if (timeout == 0) {
		return epoll_wait_f(co->epollfd, events, maxevents, 0);
	}

	schedule_sched_waitR(co, co->epollfd);
	schedule_sched_sleep(co, timeout);
	yield(co);
	schedule_cancel_sleep(co);
	schedule_cancel_wait(co, co->epollfd);

	return epoll_wait_f(co->epollfd, events, maxevents, 0);
}

int
dyco_epoll_add(int fd, struct epoll_event *ev)
{
	dyco_schedule *sched = get_sched();
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
	DYCO_MUST(epoll_ctl(co->epollfd, EPOLL_CTL_ADD, fd, ev) == 0);
	return 0;
}

int
dyco_epoll_del(int fd, struct epoll_event *ev)
{
	dyco_schedule *sched = get_sched();
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
	DYCO_MUST(epoll_ctl(co->epollfd, EPOLL_CTL_DEL, fd, ev) == 0);

	return 0;
}

void
dyco_epoll_destroy()
{
	dyco_schedule *sched = get_sched();
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

	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return -1;
	}
	assert(!TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));

	schedule_sched_waitR(co, epfd);
	schedule_sched_sleep(co, timeout);
	yield(co);
	schedule_cancel_sleep(co);
	schedule_cancel_wait(co, epfd);

	return epoll_wait_f(epfd, events, maxevents, 0);
}

int 
poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	if (timeout == 0) {
		return poll_f(fds, nfds, 0);
	}

	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return -1;
	}
	assert(!TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));

	struct epoll_event ev;

	int epfd = epoll_create(nfds);
	DYCO_MUSTNOT(epfd == -1);

	int i, ret;
	for (i = 0; i < nfds; i++) {
		ev.events = fds[i].events;
		ev.data.fd = fds[i].fd;
		DYCO_MUST(epoll_ctl(epfd, EPOLL_CTL_ADD, fds[i].fd, &ev) == 0);
	}
	
	schedule_sched_waitR(co, epfd);
	schedule_sched_sleep(co, timeout);
	yield(co);
	schedule_cancel_sleep(co);
	schedule_cancel_wait(co, epfd);

	for (i = 0; i < nfds; i++) {
		DYCO_MUST(epoll_ctl(epfd, EPOLL_CTL_DEL, fds[i].fd, NULL) == 0);
	}

	return poll_f(fds, nfds, timeout);
}

#endif
