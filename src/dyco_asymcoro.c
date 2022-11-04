#include "dyco_coroutine.h"

static void 
_exec(void *c) {
	dyco_coroutine *co = (dyco_coroutine*)c;
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	co->func(co->arg);
	SETBIT(co->status, COROUTINE_STATUS_EXITED);
	dyco_asymcoro_yield();
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

int
dyco_asymcoro_create(proc_coroutine func, void *arg)
{
	dyco_coroutine *co = dyco_coroutine_new();
	if (co == NULL) {
		printf("Failed to allocate memory for new coroutine\n");
		return -2;
	}

	co->func = func;
	co->arg = arg;
	SETBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC);
	
	return co->cid;
}


int
dyco_asymcoro_resume(int cid)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co_next = _htable_find(&sched->cid_co_map, cid);
	if ((co_next == NULL) || (!TESTBIT(co_next->status, COROUTINE_FLAGS_ASYMMETRIC))) {
		return -1;
	}
	if (TESTBIT(co_next->status, COROUTINE_STATUS_EXITED)) {
		return 0;
	}

	if (TESTBIT(co_next->status, COROUTINE_STATUS_NEW)) {
		_init_coro(co_next);
	} else {
		_load_stack(co_next);
	}

	++co_next->sched_count;

	dyco_coroutine *co_prev = sched->curr_thread;
	sched->curr_thread = co_next;
	SETBIT(co_next->status, COROUTINE_STATUS_RUNNING);
	swapcontext(&co_next->ret, &co_next->ctx);
	sched->curr_thread = co_prev;

	if (TESTBIT(co_next->status, COROUTINE_STATUS_EXITED)) {
		// if (TESTBIT(co_next->status, COROUTINE_FLAGS_INCOROPOOL))
		// 	dyco_coropool_return(co_next);
		// else
		// 	dyco_coroutine_free(co_next);
		return 0;
	}
	return cid;
}


void
dyco_asymcoro_yield()
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return;
	}
	dyco_coroutine *co = sched->curr_thread;
	assert(co != NULL);

	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED) == 0) {
		_save_stack(co);
	}
	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	swapcontext(&co->ctx, &co->ret);
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
}


void dyco_asymcoro_free(int cid)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return;
	}
	dyco_coroutine *co = _htable_find(&sched->cid_co_map, cid);
	if ((co == NULL) || (!TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC))) {
		assert(0);
		return;
	}

	--co->sched->coro_count;
	_htable_delete(&co->sched->cid_co_map, co->cid, NULL);

	if (TESTBIT(co->status, COROUTINE_FLAGS_ALLOCSTACKMEM)) {
		free(co->stack);
		co->stack = NULL;
	}
	if (co) {
		free(co);
	}
	return;
}


int dyco_asymcoro_coroID()
{
	return dyco_coroutine_coroID();
}


int dyco_asymcoro_setStack(int cid, void *stackptr, size_t stacksize)
{
	return dyco_coroutine_setStack(cid, stackptr, stacksize);
}


int dyco_asymcoro_getStack(int cid, void **stackptr, size_t *stacksize)
{
	return dyco_coroutine_getStack(cid, stackptr, stacksize);
}


int dyco_asymcoro_setUdata(int cid, void *udata)
{
	return dyco_coroutine_setUdata(cid, udata);
}


int dyco_asymcoro_getUdata(int cid, void **udata)
{
	return dyco_coroutine_getUdata(cid, udata);
}

