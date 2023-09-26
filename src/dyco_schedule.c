#include "dyco/dyco_coroutine.h"

atomic_uint sched_id_gen;

RB_GENERATE(dyco_coroutine_rbtree_sleep, dyco_coroutine, sleep_node, coroutine_sleep_cmp);

#define SCHEDULE_ISDONE(sched)		(HTABLE_EMPTY(&sched->fd_co_map) && \
				 	RB_EMPTY(&sched->sleeping) && \
				 	TAILQ_EMPTY(&sched->ready) && \
					TAILQ_EMPTY(&sched->urgent_ready))

extern void dyco_coropool_return(dyco_coroutine *co);

static dyco_coroutine*
schedule_get_expired(dyco_schedule *sched)
{
	uint64_t tdiff_usecs = diff_usecs(sched->birth, usec_now());
	dyco_coroutine *co = RB_MIN(dyco_coroutine_rbtree_sleep, &sched->sleeping);

	if (co == NULL)
		return NULL;
	if (co->sleep_usecs <= tdiff_usecs)
	{
		RB_REMOVE(dyco_coroutine_rbtree_sleep, &co->sched->sleeping, co);
		CLRBIT(co->status, COROUTINE_STATUS_SLEEPING);
		SETBIT(co->status, COROUTINE_FLAGS_EXPIRED);
		return co;
	}
	return NULL;
}

static uint64_t 
schedule_min_timeout(dyco_schedule *sched)
{
	dyco_coroutine *co = RB_MIN(dyco_coroutine_rbtree_sleep, &sched->sleeping);
	if (!co)
		return sched->loopwait_timeout;

	uint64_t now_relative_usecs = diff_usecs(sched->birth, usec_now());
	return co->sleep_usecs > now_relative_usecs ? co->sleep_usecs - now_relative_usecs : 0;
}

static int 
schedule_epoll_wait(dyco_schedule *sched)
{
	if (HTABLE_EMPTY(&sched->fd_co_map) && RB_EMPTY(&sched->sleeping)) return 0;

	if (!TAILQ_EMPTY(&sched->ready) || !TAILQ_EMPTY(&sched->urgent_ready))
		return epoll_wait_f(sched->epollfd, sched->eventlist, DYCO_MAX_EVENTS, 0);
		// return 0;

	uint64_t timeout = schedule_min_timeout(sched);
	if (timeout == 0)
		return epoll_wait_f(sched->epollfd, sched->eventlist, DYCO_MAX_EVENTS, 0);
	
	int nready = -1;
	while (1)
	{
		nready = epoll_wait_f(sched->epollfd, sched->eventlist, DYCO_MAX_EVENTS, timeout/1000);
		if (nready == -1)
		{
			if (errno == EINTR)
				continue;
			else
				DYCO_ABORT();
		}
		break;
	}
	return nready;
}

dyco_coroutine*
schedule_get_waitco(dyco_schedule *sched, int fd)
{

	return (dyco_coroutine*)htable_find(&sched->fd_co_map, fd);
}

void 
dyco_schedule_free(dyco_schedule *sched)
{
	if (sched->epollfd > 0)
	{
		close(sched->epollfd);
	}
	if (sched->stack != NULL)
	{
		free(sched->stack);
	}
	htable_clear(&sched->fd_co_map);
	htable_clear(&sched->cid_co_map);

	free(sched);

	int ret = pthread_setspecific(global_sched_key, NULL);
	assert(ret == 0);
}

int 
dyco_schedule_create(size_t stack_size, uint64_t loopwait_timeout)
{
	dyco_schedule *sched = (dyco_schedule *)calloc(1, sizeof(dyco_schedule));
	if (sched == NULL)
	{
		printf("Failed to initialize scheduler\n");
		return -1;
	}
	int ret = pthread_setspecific(global_sched_key, sched);
	assert(ret == 0);

	sched->epollfd = epoll_create(1024);
	DYCO_MUSTNOT(sched->epollfd == -1);

	int sched_stack_size = stack_size ? stack_size : DYCO_MAX_STACKSIZE;
	sched->stack_size = sched_stack_size;
	int page_size = getpagesize();
	ret = posix_memalign(&sched->stack, page_size, sched->stack_size);
	assert(ret == 0);

	pthread_t tid = pthread_self();
	sched->sched_id = ++sched_id_gen;
	sched->loopwait_timeout = loopwait_timeout ? loopwait_timeout : DYCO_DEFAULT_TIMEOUT;
	sched->birth = usec_now();
	sched->coro_count = 0;
	// sched->_cid_gen = (sched->sched_id % 0xf0) * 1000000;
	sched->_cid_gen = 0;
	// sched->_cid_gen
	sched->status = SCHEDULE_STATUS_READY;
	sched->curr_thread = NULL;

	sched->udata = NULL;
	
	TAILQ_INIT(&sched->urgent_ready);
	TAILQ_INIT(&sched->ready);
	RB_INIT(&sched->sleeping);

	htable_init(&sched->fd_co_map, 0);
	htable_init(&sched->cid_co_map, 0);

	return sched->sched_id;
}

void schedule_sched_sleep(dyco_coroutine *co, int msecs)
{
	dyco_coroutine *co_tmp = RB_FIND(dyco_coroutine_rbtree_sleep, &co->sched->sleeping, co);
	if (co_tmp != NULL)
	{
		RB_REMOVE(dyco_coroutine_rbtree_sleep, &co->sched->sleeping, co_tmp);
	}

	if (msecs <= 0) return;
	uint64_t usecs = msecs * 1000u;

	co->sleep_usecs = diff_usecs(co->sched->birth, usec_now()) + usecs;
	while (msecs)
	{
		co_tmp = RB_INSERT(dyco_coroutine_rbtree_sleep, &co->sched->sleeping, co);
		if (co_tmp)
		{
			co->sleep_usecs++;
			continue;
		}
		SETBIT(co->status, COROUTINE_STATUS_SLEEPING);
		CLRBIT(co->status, COROUTINE_FLAGS_EXPIRED);
		break;
	}

	return;
}

void
schedule_cancel_sleep(dyco_coroutine *co)
{
	if (TESTBIT(co->status, COROUTINE_STATUS_SLEEPING))
	{
		RB_REMOVE(dyco_coroutine_rbtree_sleep, &co->sched->sleeping, co);
		CLRBIT(co->status, COROUTINE_STATUS_SLEEPING);
	}

	return;
}

int
schedule_cancel_wait(dyco_coroutine *co, int fd)
{
	if (TESTBIT(co->status, COROUTINE_FLAGS_WAITING)) {

		htable_delete(&co->sched->fd_co_map, fd, NULL);
		CLRBIT(co->status, COROUTINE_FLAGS_WAITING);
		DYCO_MUST(epoll_ctl(co->sched->epollfd, EPOLL_CTL_DEL, fd, NULL) == 0);
		return 0;
	}
	return 1;
}
	

void
schedule_sched_wait(dyco_coroutine *co, int fd, unsigned int events)
{
	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = events;
	DYCO_MUST(epoll_ctl(co->sched->epollfd, EPOLL_CTL_ADD, fd, &ev) == 0);

	int ret = htable_insert(&co->sched->fd_co_map, fd, co);
	assert(ret >= 0);
	SETBIT(co->status, COROUTINE_FLAGS_WAITING);
	return;
}

void
schedule_sched_waitR(dyco_coroutine *co, int fd)
{
	schedule_sched_wait(co, fd, EPOLLIN | EPOLLHUP | EPOLLERR);
	return;
}

void
schedule_sched_waitW(dyco_coroutine *co, int fd)
{
	schedule_sched_wait(co, fd, EPOLLOUT | EPOLLHUP | EPOLLERR);
	return;
}

void
schedule_sched_waitRW(dyco_coroutine *co, int fd)
{
	schedule_sched_wait(co, fd, EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR);
	return;
}

void
schedule_stop(dyco_schedule *sched)
{
	sched->status = SCHEDULE_STATUS_STOPPED;
	sched->curr_thread = NULL;
	return;
}


void 
coro_abort(void *arg)
{
	dyco_coroutine *co = arg;
	if (TESTBIT(co->status, COROUTINE_FLAGS_INCOROPOOL)) {
		if (TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC))
			dyco_asymcpool_return(co->cid);
		else
			dyco_coropool_return(co);
	} else {
		if (TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC))
			dyco_asymcoro_free(co->cid);
		else
			freecoro(co);
	}
	return;
}
void
schedule_abort(dyco_schedule *sched) // map!
{
	htable_clear_with_freecb(&sched->cid_co_map, coro_abort);
	sched->curr_thread = NULL;
	assert(sched->coro_count == 0);
	sched->status = SCHEDULE_STATUS_ABORTED;
}

int
dyco_schedule_run()
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL)
		return -2;

	DYCO_MUSTNOT(sched->status == SCHEDULE_STATUS_RUNNING);

	int idx, nready;
	dyco_coroutine *expired;
	dyco_coroutine *last_co_ready;

	if (sched->status == SCHEDULE_STATUS_ABORTED) {
		return -2;
	}
	else if (sched->status == SCHEDULE_STATUS_DONE) {
		return 0;
	}
	sched->status = SCHEDULE_STATUS_RUNNING;
	
	while (!SCHEDULE_ISDONE(sched))
	{
		// A. probing timing coroutines
		expired = NULL;
		while ((expired = schedule_get_expired(sched)) != NULL)
		{
			SETBIT(expired->status, COROUTINE_STATUS_READY);
			if (TESTBIT(expired->status, COROUTINE_FLAGS_URGENT))
				TAILQ_INSERT_TAIL(&sched->urgent_ready, expired, ready_next);
			else
				TAILQ_INSERT_TAIL(&sched->ready, expired, ready_next);
		}

		// B. processing urgent ready coroutines
		last_co_ready = TAILQ_LAST(&sched->urgent_ready, dyco_coroutine_queue);
		idx = 0;
		while (!TAILQ_EMPTY(&sched->urgent_ready))
		{
			dyco_coroutine *co = TAILQ_FIRST(&sched->urgent_ready);
			TAILQ_REMOVE(&sched->urgent_ready, co, ready_next);
			CLRBIT(co->status, COROUTINE_STATUS_READY);
			resume(co);
			if (sched->status == SCHEDULE_STATUS_STOPPED)
				return 1;
			else if (sched->status == SCHEDULE_STATUS_ABORTED)
				return -1;
			
			if (co == last_co_ready)
				break;
			if (++idx == DYCO_URGENT_MAXEXEC)
				break;
		}

		// C. processing some of the ready coroutines
		last_co_ready = TAILQ_LAST(&sched->ready, dyco_coroutine_queue);
		idx = 0;
		while (!TAILQ_EMPTY(&sched->ready))
		{
			dyco_coroutine *co = TAILQ_FIRST(&sched->ready);
			TAILQ_REMOVE(&sched->ready, co, ready_next);
			CLRBIT(co->status, COROUTINE_STATUS_READY);
			resume(co);
			if (sched->status == SCHEDULE_STATUS_STOPPED)
				return 1;
			else if (sched->status == SCHEDULE_STATUS_ABORTED)
				return -1;
			
			if (co == last_co_ready)
				break;
			if (++idx == DYCO_URGENT_MAXWAIT)
				break;		
		}

		// D. probing event-driven coroutines
		nready = schedule_epoll_wait(sched);
		if (nready == 0)
			continue;

		idx = 0;
#ifdef	DYCO_RANDOM_WAITFD		
		int idxarray[DYCO_MAX_EVENTS];
		int ridx = 0, tmp = 0;
		while (idx < nready) {
			idxarray[idx] = idx;
			++idx;
		}
		idx = nready - 1;
		while (idx > 2) {
			ridx = rand() % idx;
			tmp = idxarray[idx];
			idxarray[idx] = idxarray[ridx];
			idxarray[ridx] = tmp;
			--idx;
		}
#endif
		while (idx < nready)
		{
#ifdef	DYCO_RANDOM_WAITFD
			struct epoll_event *ev = sched->eventlist + idxarray[idx];
#else
			struct epoll_event *ev = sched->eventlist + idx;
#endif
			
			int fd = ev->data.fd;

			dyco_coroutine *co = schedule_get_waitco(sched, fd);
			if (co != NULL)
			{
				SETBIT(co->status, COROUTINE_STATUS_READY);
				if (TESTBIT(co->status, COROUTINE_FLAGS_URGENT))
					TAILQ_INSERT_TAIL(&sched->urgent_ready, co, ready_next);
				else
					TAILQ_INSERT_TAIL(&sched->ready, co, ready_next);
				
				schedule_cancel_wait(co, fd);
			}
			++idx;
		}
		
	}
	sched->status = SCHEDULE_STATUS_DONE;
	return 0;
}

int
dyco_schedule_schedID()
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	return (int)sched->sched_id;
}

int
dyco_schedule_setUdata(void *udata)
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	sched->udata = udata;
	return 0;
}


int
dyco_schedule_getUdata(void **udata)
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	*udata = sched->udata;
	return 0;
}


int
dyco_schedule_getCoroCount()
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	return sched->coro_count;
}
