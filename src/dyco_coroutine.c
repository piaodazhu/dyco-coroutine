#include "dyco_coroutine.h"

pthread_key_t global_sched_key;
static pthread_once_t sched_key_once = PTHREAD_ONCE_INIT;

static void 
_sched_key_destructor(void *data) {
	free(data);
}

static void 
_sched_key_creator(void) {
	assert(pthread_key_create(&global_sched_key, _sched_key_destructor) == 0);
	assert(pthread_setspecific(global_sched_key, NULL) == 0);
	return;
}

static void 
_exec(void *lt) {
	dyco_coroutine *co = (dyco_coroutine*)lt;
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	co->func(co->arg);
	SETBIT(co->status, COROUTINE_STATUS_EXITED);
	_yield(co);
}

static void 
_init_coro(dyco_coroutine *co) {

	getcontext(&co->ctx);
	if (TESTBIT(co->status, COROUTINE_FLAGS_OWNSTACK)) {
		co->ctx.uc_stack.ss_sp = co->stack;
		co->ctx.uc_stack.ss_size = co->stack_size;
	} else {
		co->ctx.uc_stack.ss_sp = co->sched->stack;
		co->ctx.uc_stack.ss_size = co->sched->stack_size;
	}

	makecontext(&co->ctx, (void (*)(void)) _exec, 1, (void*)co);

	CLRBIT(co->status, COROUTINE_STATUS_NEW);
	return;
}

void
_save_stack(dyco_coroutine *co) {
	if (TESTBIT(co->status, COROUTINE_FLAGS_OWNSTACK))
		return;

	char* top = co->sched->stack + co->sched->stack_size;
	char dummy = 0;
	assert(top - &dummy <= DYCO_MAX_STACKSIZE);
	if (co->stack_size < top - &dummy) {
		co->stack = realloc(co->stack, top - &dummy);
		assert(co->stack != NULL);
	}
	co->stack_size = top - &dummy;
	memcpy(co->stack, &dummy, co->stack_size);
	return;
}

static void
_load_stack(dyco_coroutine *co) {
	if (TESTBIT(co->status, COROUTINE_FLAGS_OWNSTACK))
		return;
	
	memcpy(co->sched->stack + co->sched->stack_size - co->stack_size, co->stack, co->stack_size);
	return;
}

void 
_yield(dyco_coroutine *co) {
	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED) == 0) {
		_save_stack(co);
	}
	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	// printf("tag31\n");
	swapcontext(&co->ctx, &co->sched->ctx);
	// printf("tag32\n");
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
}

int 
_resume(dyco_coroutine *co) {
	if (TESTBIT(co->status, COROUTINE_STATUS_NEW)) {
		_init_coro(co);
	}
	else {
		_load_stack(co);
	}

	printf("-- resume coro %d --\n", co->cid);
	dyco_schedule *sched = co->sched;
	++co->sched_count;
	
	sched->curr_thread = co;
	// SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	swapcontext(&sched->ctx, &co->ctx);
	while (TESTBIT(co->status, COROUTINE_STATUS_SCHEDCALL)) {
		// printf("schedcall\n");
		if (_schedule_callexec(sched)) {
			printf("return to coro ctx\n");
			swapcontext(&sched->ctx, &co->ctx);
		} else {
			return 1;
		}
	}
	// CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	sched->curr_thread = NULL;
	// printf("back\n");
	// struct epoll_event ev;
	// int ret = epoll_wait_f(sched->epollfd, &ev, 1, -1);
	// printf("ret = %d\n", ret);
	

	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED)) {
		printf("** finish coro %d **\n", co->cid);
		_schedule_cancel_sleep(co);
		dyco_coroutine_free(co);
		// printf("[i] %d coros remain, ht 1: %d, ht2: %d, rbt: %d\n", sched->coro_count, HTABLE_EMPTY(&sched->cid_co_map), HTABLE_EMPTY(&sched->fd_co_map), RB_EMPTY(&sched->sleeping));
		return -1;
	}
	return 0;
}

int
_wait_events(int fd, unsigned int events, int timeout)
{
	// events: uint32 epoll event
	// timeout: Specifying a negative value  in  timeout  means  an infinite  timeout. Specifying  a timeout of zero causes dyco_poll_inner() to return immediately
	
	// fast return
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = (short)(events & 0xffff);
	if (timeout == 0) {
		return poll(&pfd, 1, 0);
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
	ev.data.fd = fd;
	ev.events = events;
	epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, fd, &ev);
	_schedule_sched_wait(co, fd);
	_schedule_sched_sleep(co, timeout);

	_yield(co);

	_schedule_cancel_sleep(co);
	_schedule_cancel_wait(co, fd);
	epoll_ctl(sched->epollfd, EPOLL_CTL_DEL, fd, &ev);

	return poll(&pfd, 1, 0);
}

int 
dyco_coroutine_create(proc_coroutine func, void *arg) {

	assert(pthread_once(&sched_key_once, _sched_key_creator) == 0);
	dyco_schedule *sched = _get_sched();

	if (sched == NULL) {
		dyco_schedule_create(0, 0);
		
		sched = _get_sched();
		if (sched == NULL) {
			printf("Failed to create scheduler\n");
			return -1;
		}
	}

	dyco_coroutine *co = calloc(1, sizeof(dyco_coroutine));
	if (co == NULL) {
		printf("Failed to allocate memory for new coroutine\n");
		return -2;
	}

	co->func = func;
	co->arg = arg;

	co->sched = sched;

	co->stack = NULL;
	co->stack_size = 0;
	
	co->status = BIT(COROUTINE_STATUS_NEW) | BIT(COROUTINE_STATUS_READY);
	co->sched_count = 0;
	co->sleep_usecs = 0;

	if (sched->_cid_gen == INT_MAX) sched->_cid_gen = 0;
	co->cid = sched->_cid_gen++;
	assert(_htable_insert(&sched->cid_co_map, co->cid, co) >= 0);

	co->udata = NULL;

	co->epollfd = -1;
	co->sigfd = -1;

	++sched->coro_count;
	TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);

	if (sched->status == SCHEDULE_STATUS_DONE) {
		sched->status = SCHEDULE_STATUS_READY;
	}
	return co->cid;
}

void 
dyco_coroutine_free(dyco_coroutine *co) {
	if (co == NULL)
		return;

	--co->sched->coro_count;
	_htable_delete(&co->sched->cid_co_map, co->cid, NULL);

	if (TESTBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING)) {
		_schedule_cancel_wait(co, co->epollfd);
		epoll_ctl(co->sched->epollfd, EPOLL_CTL_DEL, co->epollfd, NULL);
		close(co->epollfd);
	}
	if (TESTBIT(co->status, COROUTINE_FLAGS_WAITSIGNAL)) {
		// _schedule_cancel_wait(co, co->sigfd);
		// epoll_ctl(co->sched->epollfd, EPOLL_CTL_DEL, co->sigfd, NULL);
		sigprocmask(SIG_SETMASK, &co->old_sigmask, NULL);
		close(co->sigfd);
	}
	if ((!TESTBIT(co->status, COROUTINE_FLAGS_OWNSTACK)) && co->stack) {
		free(co->stack);
		co->stack = NULL;
	}
	if (co) {
		free(co);
	}
	return;
}

void 
dyco_coroutine_sleep(uint32_t msecs) {
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return;
	}
	if (msecs == 0) {
		SETBIT(co->status, COROUTINE_STATUS_READY);
		TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);
	} else {
		_schedule_sched_sleep(co, msecs);
	}
	_yield(co);
}

int
dyco_coroutine_waitRead(int fd, int timeout)
{
	return _wait_events(fd, EPOLLIN | EPOLLERR | EPOLLHUP, timeout); 
}


int
dyco_coroutine_waitWrite(int fd, int timeout)
{
	return _wait_events(fd, EPOLLOUT | EPOLLERR | EPOLLHUP, timeout);
}

int
dyco_coroutine_waitRW(int fd, int timeout)
{
	return _wait_events(fd, EPOLLOUT | EPOLLIN | EPOLLERR | EPOLLHUP, timeout);
}


int
dyco_coroutine_setStack(int cid, void *stackptr, size_t stacksize)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = _htable_find(&sched->cid_co_map, cid);
	if ((co == NULL) || (!TESTBIT(co->status, COROUTINE_STATUS_NEW))) {
		return -1;
	}
	if ((stackptr == NULL) || (stacksize == 0)) {
		co->stack = NULL;
		co->stack_size = 0;
		CLRBIT(co->status, COROUTINE_FLAGS_OWNSTACK);
		return 0;
	}
	co->stack = stackptr;
	co->stack_size = stacksize;
	SETBIT(co->status, COROUTINE_FLAGS_OWNSTACK);
	return 1;
}

int
dyco_coroutine_getStack(int cid, void **stackptr, size_t *stacksize)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = _htable_find(&sched->cid_co_map, cid);
	if (co == NULL) {
		return -1;
	}
	if (stackptr != NULL)
		*stackptr = co->stack;
	if (stacksize != NULL)
		*stacksize = co->stack_size;
	return TESTBIT(co->status, COROUTINE_FLAGS_OWNSTACK);
}

int
dyco_coroutine_coroID()
{
	dyco_schedule *sched = _get_sched();
	if ((sched == NULL) || (sched->curr_thread == NULL)) {
		return -1;
	}
	return sched->curr_thread->cid;
}

int
dyco_coroutine_setUdata(int cid, void *udata)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = _htable_find(&sched->cid_co_map, cid);
	if (co == NULL) {
		return -1;
	}
	co->udata = udata;
	return 0;
}

int
dyco_coroutine_getUdata(int cid, void **udata)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = _htable_find(&sched->cid_co_map, cid);
	if (co == NULL) {
		return -1;
	}
	*udata = co->udata;
	return 0;
}

int
dyco_coroutine_getSchedCount(int cid)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = _htable_find(&sched->cid_co_map, cid);
	if (co == NULL) {
		return -1;
	}
	return co->sched_count;
}

