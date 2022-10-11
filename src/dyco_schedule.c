#include "dyco_coroutine.h"

RB_GENERATE(_dyco_coroutine_rbtree_sleep, _dyco_coroutine, sleep_node, _coroutine_sleep_cmp);
RB_GENERATE(_dyco_coroutine_rbtree_wait, _dyco_coroutine, wait_node, _coroutine_wait_cmp);

#define SCHEDULE_ISDONE(sched)		(RB_EMPTY(&sched->waiting) && \
				 	RB_EMPTY(&sched->sleeping) && \
				 	TAILQ_EMPTY(&sched->ready))

static dyco_coroutine*
_schedule_get_expired(dyco_schedule *sched)
{
	uint64_t t_diff_usecs = _diff_usecs(sched->birth, _usec_now());
	dyco_coroutine *co = RB_MIN(_dyco_coroutine_rbtree_sleep, &sched->sleeping);

	if (co == NULL)
		return NULL;
	if (co->sleep_usecs <= t_diff_usecs)
	{
		RB_REMOVE(_dyco_coroutine_rbtree_sleep, &co->sched->sleeping, co);
		CLRBIT(co->status, COROUTINE_STATUS_SLEEPING);
		SETBIT(co->status, COROUTINE_STATUS_EXPIRED);
		printf("sleeptree remove co %d\n", co->id);
		return co;
	}
	return NULL;
}

static uint64_t 
_schedule_min_timeout(dyco_schedule *sched)
{
	uint64_t now_relative_usecs = _diff_usecs(sched->birth, _usec_now());
	uint64_t min = sched->default_timeout;

	dyco_coroutine *co = RB_MIN(_dyco_coroutine_rbtree_sleep, &sched->sleeping);
	if (!co)
		return min;

	return co->sleep_usecs > now_relative_usecs ? co->sleep_usecs - now_relative_usecs : 0;
}

static int 
_schedule_epoll_wait(dyco_schedule *sched)
{
	sched->num_new_events = 0;
	if (!TAILQ_EMPTY(&sched->ready)) return 0;

	uint64_t timeout = _schedule_min_timeout(sched);
	if (timeout == 0) return 0;

	int nready = -1;
	while (1)
	{
		nready = epoll_wait(sched->epollfd, sched->eventlist, DYCO_MAX_EVENTS, timeout/1000);
		if (nready == -1)
		{
			if (errno == EINTR)
				continue;
			else
				assert(0);
		}
		break;
	}
	sched->num_new_events = nready;

	return 0;
}

dyco_coroutine*
_schedule_get_waitco(dyco_schedule *sched, int fd)
{
	dyco_coroutine find_it = {0};
	find_it.fd = fd;
	dyco_coroutine *co = RB_FIND(_dyco_coroutine_rbtree_wait, &sched->waiting, &find_it);
	return co;
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

	free(sched);

	assert(pthread_setspecific(global_sched_key, NULL) == 0);
}

int 
dyco_schedule_create(int stack_size)
{
	dyco_schedule *sched = (dyco_schedule *)calloc(1, sizeof(dyco_schedule));
	if (sched == NULL)
	{
		printf("Failed to initialize scheduler\n");
		abort();
	}
	assert(pthread_setspecific(global_sched_key, sched) == 0);

	sched->epollfd = epoll_create(1024);
	if (sched->epollfd == -1)
	{
		printf("Failed to initialize epoller\n");
		abort();
	}

	int sched_stack_size = stack_size ? stack_size : DYCO_MAX_STACKSIZE;
	sched->stack_size = sched_stack_size;
	int page_size = getpagesize();
	int ret = posix_memalign(&sched->stack, page_size, sched->stack_size);
	assert(ret == 0);

	sched->default_timeout = 3000000u;
	sched->birth = _usec_now();
	sched->coro_count = 0;
	sched->_id_gen = 0;
	
	TAILQ_INIT(&sched->ready);
	RB_INIT(&sched->sleeping);
	RB_INIT(&sched->waiting);

	return 0;
}

void dyco_schedule_sched_sleepdown(dyco_coroutine *co, int msecs)
{
	dyco_coroutine *co_tmp = RB_FIND(_dyco_coroutine_rbtree_sleep, &co->sched->sleeping, co);
	if (co_tmp != NULL)
	{
		RB_REMOVE(_dyco_coroutine_rbtree_sleep, &co->sched->sleeping, co_tmp);
	}

	if (msecs <= 0) return;
	uint64_t usecs = msecs * 1000u;

	co->sleep_usecs = _diff_usecs(co->sched->birth, _usec_now()) + usecs;
	while (msecs)
	{
		co_tmp = RB_INSERT(_dyco_coroutine_rbtree_sleep, &co->sched->sleeping, co);
		if (co_tmp)
		{
			co->sleep_usecs++;
			continue;
		}
		SETBIT(co->status, COROUTINE_STATUS_SLEEPING);
		CLRBIT(co->status, COROUTINE_STATUS_EXPIRED);
		break;
	}

	return;
}

void
dyco_schedule_desched_sleepdown(dyco_coroutine *co)
{
	if (TESTBIT(co->status, COROUTINE_STATUS_SLEEPING))
	{
		RB_REMOVE(_dyco_coroutine_rbtree_sleep, &co->sched->sleeping, co);
		CLRBIT(co->status, COROUTINE_STATUS_SLEEPING);
	}

	return;
}

dyco_coroutine*
dyco_schedule_desched_wait(dyco_coroutine *co, int fd)
{
	if (co == NULL) return NULL;

	RB_REMOVE(_dyco_coroutine_rbtree_wait, &co->sched->waiting, co);
	CLRBIT(co->status, COROUTINE_STATUS_WAITING);

	return co;
}

void
dyco_schedule_sched_wait(dyco_coroutine *co, int fd, unsigned int events)
{
	co->fd = fd;
	co->events = events;

	dyco_coroutine *co_tmp = RB_INSERT(_dyco_coroutine_rbtree_wait, &co->sched->waiting, co);
	SETBIT(co->status, COROUTINE_STATUS_WAITING);
	assert(co_tmp == NULL);

	return;
}

void
dyco_schedule_run(void)
{

	dyco_schedule *sched = _get_sched();
	if (sched == NULL)
		return;

	while (!SCHEDULE_ISDONE(sched))
	{
		dyco_coroutine *expired = NULL;
		while ((expired = _schedule_get_expired(sched)) != NULL)
		{
			_resume(expired);
		}

		dyco_coroutine *last_co_ready = TAILQ_LAST(&sched->ready, _dyco_coroutine_queue);
		while (!TAILQ_EMPTY(&sched->ready))
		{
			dyco_coroutine *co = TAILQ_FIRST(&sched->ready);
			TAILQ_REMOVE(&co->sched->ready, co, ready_next);
			CLRBIT(co->status, COROUTINE_STATUS_READY);
			_resume(co);
			if (co == last_co_ready)
				break;
		}

		_schedule_epoll_wait(sched);
		while (sched->num_new_events)
		{
			int idx = --sched->num_new_events;
			struct epoll_event *ev = sched->eventlist + idx;
			int fd = ev->data.fd;

			dyco_coroutine *co = _schedule_get_waitco(sched, fd);
			if (co != NULL)
			{
				_resume(co);
			}
		}
	}
	dyco_schedule_free(sched);

	return;
}
