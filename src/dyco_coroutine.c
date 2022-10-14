#include "dyco_coroutine.h"

pthread_key_t global_sched_key;
static pthread_once_t sched_key_once = PTHREAD_ONCE_INIT;
// extern int dyco_schedule_create(int stack_size, uint64_t loopwait_timeout);

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

static void
_save_stack(dyco_coroutine *co) {
	if (TESTBIT(co->status, COROUTINE_FLAGS_OWNSTACK))
		return;
	// printf("co %d save stack\n", co->cid);
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
	// printf("co %d load stack\n", co->cid);
	memcpy(co->sched->stack + co->sched->stack_size - co->stack_size, co->stack, co->stack_size);
	return;
}

void 
_yield(dyco_coroutine *co) {
	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED) == 0) {
		_save_stack(co);
	}
	// printf("co %d epoll sleep\n", co->cid);
	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	swapcontext(&co->ctx, &co->sched->ctx);
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	// printf("co %d epoll wake\n", co->cid);
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
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	swapcontext(&sched->ctx, &co->ctx);
	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	sched->curr_thread = NULL;

	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED)) {
		printf("** finish coro %d **\n", co->cid);
		_schedule_cancel_sleep(co);
		dyco_coroutine_free(co);
		return -1;
	} 
	return 0;
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

	// co->stack = malloc(65535);
	// co->stack_size = 65535;
	// SETBIT(co->status, COROUTINE_FLAGS_OWNSTACK);

	if (sched->_cid_gen == INT_MAX) sched->_cid_gen = 0;
	co->cid = sched->_cid_gen++;
	assert(_htable_insert(&sched->cid_co_map, co->cid, co) >= 0);

	co->data = NULL;

	co->fd = -1;
	co->epollfd = -1;
	
	++sched->coro_count;
	TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);

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
		_schedule_cancel_wait(co, co->sigfd);
		epoll_ctl(co->sched->epollfd, EPOLL_CTL_DEL, co->sigfd, NULL);
		close(co->epollfd);
	}
	if (co->stack) {
		free(co->stack);
		co->stack = NULL;
	}
	if (co->data) {
		free(co->data);
		co->data = NULL;
	}
	if (co) {
		free(co);
	}
	return;
}

void 
dyco_coroutine_sleep(uint32_t msecs) {
	dyco_coroutine *co = _get_sched()->curr_thread;
	if (msecs == 0) {
		SETBIT(co->status, COROUTINE_STATUS_READY);
		TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);
	} else {
		_schedule_sched_sleep(co, msecs);
	}
	_yield(co);
}

int
dyco_coroutine_setStack(int cid, void *stackptr, size_t stacksize)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = _htable_find(&sched->cid_co_map, cid);
	if (co == NULL || !TESTBIT(co->status, COROUTINE_STATUS_NEW)) {
		return -1;
	}
	if (stackptr == NULL || stacksize == 0) {
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
	*stackptr = co->stack;
	*stacksize = co->stack_size;
	return TESTBIT(co->status, COROUTINE_FLAGS_OWNSTACK);
}

int
dyco_coroutine_coroID()
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL || sched->curr_thread == NULL) {
		return -1;
	}
	return sched->curr_thread->cid;
}

int
dyco_coroutine_schedID()
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	return sched->sched_id;
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
	co->data = udata;
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
	*udata = co->data;
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

