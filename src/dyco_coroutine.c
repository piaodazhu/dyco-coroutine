#include "dyco/dyco_coroutine.h"

pthread_key_t global_sched_key;
static pthread_once_t sched_key_once = PTHREAD_ONCE_INIT;

extern void dyco_coropool_return(dyco_coroutine *co);

static void 
_sched_key_destructor(void *data)
{
	free(data);
}

static void 
_sched_key_creator(void)
{
	int ret;
	ret = pthread_key_create(&global_sched_key, _sched_key_destructor);
	assert(ret == 0);
	ret = pthread_setspecific(global_sched_key, NULL);
	assert(ret == 0);
	return;
}

static void 
_exec(void *c)
{
	dyco_coroutine *co = (dyco_coroutine*)c;
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	co->func(co->arg);
	SETBIT(co->status, COROUTINE_STATUS_EXITED);
	_yield(co);
}

static void 
_init_coro(dyco_coroutine *co)
{

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
_savestk(dyco_coroutine *co)
{
	if (TESTBIT(co->status, COROUTINE_FLAGS_OWNSTACK))
		return;

	char* top = co->sched->stack + co->sched->stack_size;
	char dummy = 0;
	int cursize = top - &dummy;
	assert(cursize <= DYCO_MAX_STACKSIZE);
	if (co->stack_size < cursize) {
		co->stack = realloc(co->stack, cursize);
		assert(co->stack != NULL);
	}
	co->stack_size = cursize;
	memcpy(co->stack, &dummy, co->stack_size);
	SETBIT(co->status, COROUTINE_FLAGS_ALLOCSTACKMEM);
	return;
}

void
_loadstk(dyco_coroutine *co)
{
	if (TESTBIT(co->status, COROUTINE_FLAGS_OWNSTACK))
		return;
	
	memcpy(co->sched->stack + co->sched->stack_size - co->stack_size, co->stack, co->stack_size);
	return;
}

void 
_yield(dyco_coroutine *co)
{
	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED) == 0) {
		_savestk(co);
	}
	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	swapcontext(&co->ctx, &co->sched->ctx);
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
}

int 
_resume(dyco_coroutine *co)
{
	if (TESTBIT(co->status, COROUTINE_STATUS_NEW)) {
		_init_coro(co);
	}
	else {
		_loadstk(co);
	}

	dyco_schedule *sched = co->sched;
	++co->sched_count;
	
	sched->curr_thread = co;
	swapcontext(&sched->ctx, &co->ctx);
	while (TESTBIT(co->status, COROUTINE_STATUS_SCHEDCALL)) {
		if (_schedule_callexec(sched)) {
			swapcontext(&sched->ctx, &co->ctx);
		} else {
			return 1;
		}
	}
	sched->curr_thread = NULL;

	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED)) {
		_schedule_cancel_sleep(co);
		if (TESTBIT(co->status, COROUTINE_FLAGS_INCOROPOOL))
			dyco_coropool_return(co);
		else
			_freecoro(co);
		
		return -1;
	}
	return 0;
}

int
_waitev(int fd, unsigned int events, int timeout)
{
	// events: uint32 epoll event
	// timeout: Specifying a negative value  in  timeout  means  an infinite  timeout. Specifying  a timeout of zero causes dyco_poll_inner() to return immediately
	
	// fast return
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = (short)(events & 0xffff);
	if (timeout == 0) {
		return poll_f(&pfd, 1, 0);
	}

	dyco_schedule *sched = _get_sched();
	DYCO_MUST(sched != NULL);
	dyco_coroutine *co = sched->curr_thread;
	DYCO_MUST(co != NULL);
	DYCO_MUSTNOT(TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));
	
	assert(!TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));
	
	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = events;
	DYCO_MUST(epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, fd, &ev) == 0);
	_schedule_sched_wait(co, fd);
	_schedule_sched_sleep(co, timeout);

	_yield(co);

	_schedule_cancel_sleep(co);
	_schedule_cancel_wait(co, fd);
	DYCO_MUST(epoll_ctl(sched->epollfd, EPOLL_CTL_DEL, fd, NULL) == 0);

	return poll_f(&pfd, 1, 0);
}

int 
dyco_coroutine_create(proc_coroutine func, void *arg) 
{
	dyco_coroutine *co = _newcoro();
	if (co == NULL) {
		printf("Failed to allocate memory for new coroutine\n");
		return -2;
	}

	co->func = func;
	co->arg = arg;
	
	dyco_schedule *sched = _get_sched();
	co->sched = sched;
	// co->cid = sched->_cid_gen++;
	sched->_cid_gen = (sched->_cid_gen + 1) & 0xffffff;
	co->cid = ((sched->sched_id & 0xff) << 24) | sched->_cid_gen;

	int ret = _htable_insert(&sched->cid_co_map, co->cid, co);
	DYCO_MUST(ret >= 0);
	++sched->coro_count;
	
	TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);

	if (co->sched->status == SCHEDULE_STATUS_DONE) {
		co->sched->status = SCHEDULE_STATUS_READY;
	}
	return co->cid;
}

dyco_coroutine*
_newcoro()
{
	int ret = pthread_once(&sched_key_once, _sched_key_creator);
	DYCO_MUST(ret == 0);
	dyco_schedule *sched = _get_sched();

	if (sched == NULL) {
		dyco_schedule_create(0, 0);
		
		sched = _get_sched();
		if (sched == NULL) {
			printf("Failed to create scheduler\n");
			return NULL;
		}
	}

	dyco_coroutine *co = calloc(1, sizeof(dyco_coroutine));
	if (co == NULL) {
		printf("Failed to allocate memory for new coroutine\n");
		return NULL;
	}

	co->stack = NULL;
	co->stack_size = 0;
	
	co->status = BIT(COROUTINE_STATUS_NEW) | BIT(COROUTINE_STATUS_READY);
	co->sched_count = 0;
	co->sleep_usecs = 0;

	co->cpool = NULL;

	co->udata = NULL;

	co->epollfd = -1;
	co->sigfd = -1;
	
	return co;
}
void 
_freecoro(dyco_coroutine *co) {
	if (co == NULL)
		return;

	--co->sched->coro_count;
	_htable_delete(&co->sched->cid_co_map, co->cid, NULL);

	if (TESTBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING)) {
		_schedule_cancel_wait(co, co->epollfd);
		DYCO_MUST(epoll_ctl(co->sched->epollfd, EPOLL_CTL_DEL, co->epollfd, NULL) == 0);
		close(co->epollfd);
	}
	if (TESTBIT(co->status, COROUTINE_FLAGS_WAITSIGNAL)) {
		sigprocmask(SIG_SETMASK, &co->old_sigmask, NULL);
		close(co->sigfd);
	}
	if (TESTBIT(co->status, COROUTINE_FLAGS_ALLOCSTACKMEM)) {
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
	DYCO_MUST(sched != NULL);
	dyco_coroutine *co = sched->curr_thread;
	DYCO_MUST(co != NULL);
	DYCO_MUSTNOT(TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));

	assert(!TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));

	if (msecs == 0) {
		SETBIT(co->status, COROUTINE_STATUS_READY);
		TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);
		// printf("insert to ready queue\n");
	} else {
		_schedule_sched_sleep(co, msecs);
	}
	// printf("yed\n");
	_yield(co);
}

int
dyco_coroutine_waitRead(int fd, int timeout)
{
	return _waitev(fd, EPOLLIN | EPOLLERR | EPOLLHUP, timeout); 
}


int
dyco_coroutine_waitWrite(int fd, int timeout)
{
	return _waitev(fd, EPOLLOUT | EPOLLERR | EPOLLHUP, timeout);
}

int
dyco_coroutine_waitRW(int fd, int timeout)
{
	return _waitev(fd, EPOLLOUT | EPOLLIN | EPOLLERR | EPOLLHUP, timeout);
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

