#include "dyco/dyco_coroutine.h"

#define dyco_coropool_available(cp)	((cp)->totalsize - (cp)->activenum)

dyco_coroutine*
get_coro_by_cid(int cid) 
{
	dyco_schedule *sched = get_sched();
	assert(sched != NULL);
	return htable_find(&sched->cid_co_map, cid);
}

int
coro_setstack(dyco_coroutine *co, void *stackptr, size_t stacksize)
{
	if ((co == NULL) || (!TESTBIT(co->status, COROUTINE_STATUS_NEW))) {
		return -1;
	}
	if (stacksize == 0) {
		co->stack = NULL;
		co->stack_size = 0;
		CLRBIT(co->status, COROUTINE_FLAGS_OWNSTACK);
		return 0;
	}
	if (stackptr == NULL) {
		int page_size = getpagesize();
		co->stack_size = stacksize + stacksize % page_size;
		int ret = posix_memalign(&co->stack, page_size, co->stack_size);
		assert(ret == 0);
		SETBIT(co->status, COROUTINE_FLAGS_OWNSTACK);
		SETBIT(co->status, COROUTINE_FLAGS_ALLOCSTACKMEM);
		return 1;
	}
	co->stack = stackptr;
	co->stack_size = stacksize;
	SETBIT(co->status, COROUTINE_FLAGS_OWNSTACK);
	return 1;
}

static void
cp_notify(int fd)
{
	DYCO_MUST(eventfd_write(fd, 1) == 0);
	return;
}

static int
cp_wait(dyco_coropool* cp, int timeout)
{
	int remain = dyco_coropool_available(cp);
	if (remain > 0 || timeout == 0) {
		return remain;
	}

	dyco_schedule *sched = get_sched();
	DYCO_MUSTNOT(sched == NULL);

	dyco_coroutine *co = sched->curr_thread;
	DYCO_MUSTNOT(co == NULL);
	DYCO_MUSTNOT(TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));

	int notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	DYCO_MUSTNOT(notifyfd == -1);

	dyco_sublist *notify = (dyco_sublist*)malloc(sizeof(dyco_sublist));
	if (notify == NULL)
		return 0;
	notify->notifyfd = notifyfd;
	notify->next = NULL;
	if (cp->sublist == NULL) cp->sublist = notify;
	else {
		dyco_sublist *ptr = cp->sublist;
		while (ptr->next != NULL) ptr = ptr->next;
		ptr->next = notify;
	}

	schedule_sched_waitR(co, notifyfd);
	schedule_sched_sleep(co, timeout);
	yield(co);
	schedule_cancel_sleep(co);
	schedule_cancel_wait(co, notifyfd);

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
cp_create(int totalsize, size_t stacksize, int isasymmetric)
{
	if (totalsize <= 0) return NULL;
	dyco_coropool *cp = (dyco_coropool*)malloc(sizeof(dyco_coropool));
	if (cp == NULL) 
		return NULL;
	SLIST_INIT(&cp->freelist);
	
	int i, cid;
	dyco_coroutine *cur;
	for (i = 0; i < totalsize; ++i) {
		cur = newcoro();
		if (isasymmetric != 0)
			SETBIT(cur->status, COROUTINE_FLAGS_ASYMMETRIC);

		if (isasymmetric != 0 || stacksize > 0)
			coro_setstack(cur, NULL, stacksize);

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
cp_resize(dyco_coropool* cp, int newsize, int isasymmetric)
{
	if (cp == NULL || newsize == cp->totalsize)
		return cp;
	
	int i, cid, n;
	dyco_coroutine *cur;
	if (cp->totalsize < newsize) {
		// increase coros
		for (i = cp->totalsize; i < newsize; ++i) {
			cur = newcoro();
			if (isasymmetric != 0)
				SETBIT(cur->status, COROUTINE_FLAGS_ASYMMETRIC);

			if (isasymmetric != 0 || cp->stacksize > 0)
				coro_setstack(cur, NULL, cp->stacksize );
			
			SLIST_INSERT_HEAD(&cp->freelist, cur, cpool_next);
			SETBIT(cur->status, COROUTINE_FLAGS_INCOROPOOL);
			cur->cpool = cp;

			if (cp->sublist != NULL) {
				// notify
				dyco_sublist *head = cp->sublist;
				cp_notify(head->notifyfd);
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
			if (isasymmetric != 0)
				freecoro(cur);
			else
				dyco_asymcoro_free(cur->cid);
		}
		cp->totalsize = newsize;
	}
	return cp;
}


int
cp_destroy(dyco_coropool** coropool, int isasymmetric)
{
	dyco_coropool* cp = *coropool;
	if (cp == NULL) {
		return 0;
	}

	if (cp->activenum != 0) return -1;
	int i;
	dyco_coroutine *cur;
	for (i = 0; i < cp->totalsize; ++i) {
		cur = SLIST_FIRST(&cp->freelist);
		SLIST_REMOVE_HEAD(&cp->freelist, cpool_next);
		if (isasymmetric != 0)
			freecoro(cur);
		else
			dyco_asymcoro_free(cur->cid);
	}
	free(cp);
	return 0;
}

int
cp_obtain(dyco_coropool* cp, proc_coroutine func, void *arg, int timeout, int isasymmetric, int isurgent)
{
	DYCO_MUST(func != NULL);
	
	dyco_schedule *sched = get_sched();
	assert(sched != NULL);

	int ret = cp_wait(cp, timeout);
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
	if (isasymmetric != 0)
		SETBIT(newstatus, COROUTINE_FLAGS_ASYMMETRIC);
	co->status = newstatus;

	co->sched_count = 0;
	co->sleep_usecs = 0;
	co->udata = NULL;
	co->epollfd = -1;
	co->sigfd = -1;

	co->sched = sched;
	// co->cid = sched->_cid_gen++;
	sched->_cid_gen = (sched->_cid_gen + 1) & 0xffffff;
	co->cid = ((sched->sched_id & 0xff) << 24) | sched->_cid_gen;
	ret = htable_insert(&sched->cid_co_map, co->cid, co);
	assert(ret >= 0);
	++sched->coro_count;

	if (isasymmetric == 0) {
		if (isurgent) {
			SETBIT(co->status, COROUTINE_FLAGS_URGENT);
			TAILQ_INSERT_TAIL(&sched->urgent_ready, co, ready_next);
		} else {
			TAILQ_INSERT_TAIL(&sched->ready, co, ready_next);
		}
		
		if (sched->status == SCHEDULE_STATUS_DONE) {
			sched->status = SCHEDULE_STATUS_READY;
		}
	}

	return co->cid;
}


void
cp_return(dyco_coroutine *co, int isasymmetric)
{
	if (co == NULL) {
		return;
	}
	dyco_coropool* cp = co->cpool;
	if (cp == NULL) {
		return;
	}

	--co->sched->coro_count;
	htable_delete(&co->sched->cid_co_map, co->cid, NULL);

	if (TESTBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING)) {
		schedule_cancel_wait(co, co->epollfd);
		DYCO_MUST(epoll_ctl(co->sched->epollfd, EPOLL_CTL_DEL, co->epollfd, NULL) == 0);
		close(co->epollfd);
	}
	if (TESTBIT(co->status, COROUTINE_FLAGS_WAITSIGNAL)) {
		sigprocmask(SIG_SETMASK, &co->old_sigmask, NULL);
		close(co->sigfd);
	}
	CLRBIT(co->status, COROUTINE_FLAGS_URGENT);

	SLIST_INSERT_HEAD(&cp->freelist, co, cpool_next);
	--cp->activenum;
	if (cp->sublist != NULL) {
		dyco_sublist *head = cp->sublist;
		cp_notify(head->notifyfd);
		cp->sublist = head->next;
	}
	return;
}


dyco_coropool*
dyco_coropool_create(int totalsize, size_t stacksize)
{
	return cp_create(totalsize, stacksize, 0);
}


dyco_coropool*
dyco_coropool_resize(dyco_coropool* cp, int newsize)
{
	return cp_resize(cp, newsize, 0);
}


int
dyco_coropool_destroy(dyco_coropool** cp)
{
	return cp_destroy(cp, 0);
}

int
dyco_coropool_obtain(dyco_coropool* cp, proc_coroutine func, void *arg, int timeout)
{
	return cp_obtain(cp, func, arg, timeout, 0, 0);
}

int
dyco_coropool_obtain_urgent(dyco_coropool* cp, proc_coroutine func, void *arg, int timeout)
{
	return cp_obtain(cp, func, arg, timeout, 0, 1);
}



void
dyco_coropool_return(dyco_coroutine *co)
{
	cp_return(co, 0);
}

dyco_coropool*
dyco_asymcpool_create(int totalsize, size_t stacksize)
{
	return cp_create(totalsize, stacksize, 1);
}


dyco_coropool*
dyco_asymcpool_resize(dyco_coropool* cp, int newsize)
{
	return cp_resize(cp, newsize, 1);
}


int
dyco_asymcpool_destroy(dyco_coropool** cp)
{
	return cp_destroy(cp, 1);
}


int
dyco_asymcpool_available(dyco_coropool* cp)
{
	return dyco_coropool_available(cp);
}


int
dyco_asymcpool_obtain(dyco_coropool* cp, proc_coroutine func, void *arg, int timeout)
{
	return cp_obtain(cp, func, arg, timeout, 1, 0);
}

void
dyco_asymcpool_return(int cid)
{
	dyco_coroutine *co = get_coro_by_cid(cid);
	if (co != NULL)
		cp_return(co, 1);
}
