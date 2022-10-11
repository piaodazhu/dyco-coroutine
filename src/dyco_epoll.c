#include <sys/eventfd.h>

#include "dyco_coroutine.h"


// int dyco_epoll_create(void) {
// 	return epoll_create(1024);
// } 

// int dyco_epoll_wait(struct timespec t) {
// 	dyco_schedule *sched = _get_sched();
// 	return epoll_wait(sched->epollfd, sched->eventlist, DYCO_MAX_EVENTS, t.tv_sec*1000.0 + t.tv_nsec/1000000.0);
// }


