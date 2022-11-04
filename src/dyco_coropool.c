#include "dyco_coroutine.h"

#define dyco_coropool_available(cp)	((cp)->totalsize - (cp)->activenum)

dyco_coroutine*
_get_coro_by_cid(int cid) 
{
	dyco_schedule *sched = _get_sched();
	assert(sched != NULL);
	return _htable_find(&sched->cid_co_map, cid);
}

static void
_cp_notify(int fd)
{
	eventfd_write(fd, 1);
	return;
}

static int
_cp_wait(dyco_coropool* cp, int timeout)
{
	int remain = dyco_coropool_available(cp);
	if (remain > 0 || timeout == 0) {
		return remain;
	}

	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return -1;
	}

	int notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	dyco_sublist *notify = (dyco_sublist*)malloc(sizeof(dyco_sublist));
	notify->notifyfd = notifyfd;
	notify->next = NULL;
	if (cp->sublist == NULL) cp->sublist = notify;
	else {
		dyco_sublist *ptr = cp->sublist;
		while (ptr->next != NULL) ptr = ptr->next;
		ptr->next = notify;
	}

	struct epoll_event ev;
	ev.data.fd = notifyfd;
	ev.events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET;
	epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, notifyfd, &ev);
	_schedule_sched_wait(co, notifyfd);
	_schedule_sched_sleep(co, timeout);
	_yield(co);
	_schedule_cancel_sleep(co);
	_schedule_cancel_wait(co, notifyfd);
	epoll_ctl(sched->epollfd, EPOLL_CTL_DEL, notifyfd, NULL);

	eventfd_t count;
	int ret;
    	ret = eventfd_read(notifyfd, &count);
	
	// timeout
	if (ret < 0) {
		if (cp->sublist == notify) {
			cp->sublist = notify->next;
		} else {
			dyco_sublist *ptr = cp->sublist;
			while (ptr->next != NULL) {
				if (ptr->next == notify) {
					ptr->next = notify->next;
					break;
				}
				ptr = ptr->next;
			}
		}
	}
	
	close(notifyfd);
	free(notify);
	return dyco_coropool_available(cp);
}


dyco_coropool*
dyco_coropool_create(int totalsize, size_t stacksize)
{
	if (totalsize <= 0) return NULL;
	dyco_coropool *cp = (dyco_coropool*)malloc(sizeof(dyco_coropool));
	if (cp == NULL) return NULL;
	SLIST_INIT(&cp->freelist);
	
	int i, cid;
	dyco_coroutine *cur;
	for (i = 0; i < totalsize; ++i) {
		cid = dyco_coroutine_create(NULL, NULL);
		assert(cid >= 0);

		if (stacksize > 0)
			dyco_coroutine_setStack(cid, NULL, stacksize);
		
		cur = _get_coro_by_cid(cid);
		SLIST_INSERT_HEAD(&cp->freelist, cur, cpool_next);
		SETBIT(cur->status, COROUTINE_FLAGS_INCOROPOOL);
		cur->cpool = cp;
	}

	cp->totalsize = totalsize;
	cp->activenum = 0;
	cp->stacksize = stacksize;
	cp->sublist = NULL;

	return cp;
}


dyco_coropool*
dyco_coropool_resize(dyco_coropool* cp, int newsize)
{
	if (cp == NULL || newsize == cp->totalsize)
		return cp;
	
	int i, cid, n;
	dyco_coroutine *cur;
	if (cp->totalsize < newsize) {
		// increase coros
		for (i = cp->totalsize; i < newsize; ++i) {
			cid = dyco_coroutine_create(NULL, NULL);
			assert(cid >= 0);

			if (cp->stacksize > 0)
				dyco_coroutine_setStack(cid, NULL, cp->stacksize);
			
			cur = _get_coro_by_cid(cid);
			SLIST_INSERT_HEAD(&cp->freelist, cur, cpool_next);
			SETBIT(cur->status, COROUTINE_FLAGS_INCOROPOOL);
			cur->cpool = cp;

			if (cp->sublist != NULL) {
				// notify
				dyco_sublist *head = cp->sublist;
				_cp_notify(head->notifyfd);
				cp->sublist = head->next;
			}
		}
		cp->totalsize = newsize;
	} 
	else
	{
		// decrease coros
		n = newsize >= cp->activenum ? cp->totalsize - newsize : dyco_coropool_available(cp);
		for (i = 0; i < n; ++i) {
			cur = SLIST_FIRST(&cp->freelist);
			SLIST_REMOVE_HEAD(&cp->freelist, cpool_next);
			dyco_coroutine_free(cur);
		}
		cp->totalsize = newsize;
	}
	return cp;
}


int
dyco_coropool_destroy(dyco_coropool** _cp)
{
	dyco_coropool* cp = *_cp;
	if (cp == NULL) {
		return 0;
	}

	if (cp->activenum != 0) return -1;
	int i;
	dyco_coroutine *cur;
	for (i = 0; i < cp->totalsize; ++i) {
		cur = SLIST_FIRST(&cp->freelist);
		SLIST_REMOVE_HEAD(&cp->freelist, cpool_next);
		dyco_coroutine_free(cur);
	}
	free(cp);
	return 0;
}

int
dyco_coropool_obtain(dyco_coropool* cp, proc_coroutine func, void *arg, int timeout)
{
	if (func == NULL) 
		return -1;
	
	int ret = _cp_wait(cp, timeout);
	if (ret <= 0) {
		return ret;
	}

	dyco_coroutine *co = SLIST_FIRST(&cp->freelist);
	SLIST_REMOVE_HEAD(&cp->freelist, cpool_next);

	++cp->activenum;

	co->func = func;
	co->arg = arg;

	uint32_t newstatus = BIT(COROUTINE_STATUS_NEW) | BIT(COROUTINE_STATUS_READY) | BIT(COROUTINE_FLAGS_INCOROPOOL);
	if (TESTBIT(co->status, COROUTINE_FLAGS_OWNSTACK))
		SETBIT(newstatus, COROUTINE_FLAGS_OWNSTACK);
	if (TESTBIT(co->status, COROUTINE_FLAGS_ALLOCSTACKMEM))
		SETBIT(newstatus, COROUTINE_FLAGS_ALLOCSTACKMEM);
	co->status = newstatus;

	co->sched_count = 0;
	co->sleep_usecs = 0;
	co->udata = NULL;
	co->epollfd = -1;
	co->sigfd = -1;

	TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);
	if (co->sched->status == SCHEDULE_STATUS_DONE) {
		co->sched->status = SCHEDULE_STATUS_READY;
	}

	return co->cid;
}


void
dyco_coropool_return(dyco_coroutine *co)
{
	if (co == NULL) {
		return;
	}
	dyco_coropool* cp = co->cpool;
	if (cp == NULL) {
		return;
	}

	if (TESTBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING)) {
		_schedule_cancel_wait(co, co->epollfd);
		epoll_ctl(co->sched->epollfd, EPOLL_CTL_DEL, co->epollfd, NULL);
		close(co->epollfd);
	}
	if (TESTBIT(co->status, COROUTINE_FLAGS_WAITSIGNAL)) {
		sigprocmask(SIG_SETMASK, &co->old_sigmask, NULL);
		close(co->sigfd);
	}

	SLIST_INSERT_HEAD(&cp->freelist, co, cpool_next);
	--cp->activenum;
	if (cp->sublist != NULL) {
		dyco_sublist *head = cp->sublist;
		_cp_notify(head->notifyfd);
		cp->sublist = head->next;
	}
	return;
}
