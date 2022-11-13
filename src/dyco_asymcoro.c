#include "dyco/dyco_coroutine.h"

int
dyco_coroutine_isasymmetric(int cid)
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -2;
	}
	dyco_coroutine *co = htable_find(&sched->cid_co_map, cid);
	if (co == NULL) {
		return -1;
	}

	return TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC);
}

static void 
execute(void *c) {
	dyco_coroutine *co = (dyco_coroutine*)c;
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	co->func(co->arg);
	SETBIT(co->status, COROUTINE_STATUS_EXITED);
	dyco_asymcoroyield();
}

static void 
init_coro(dyco_coroutine *co)
{	
	getcontext(&co->ctx);
	if (TESTBIT(co->status, COROUTINE_FLAGS_OWNSTACK)) {
		co->ctx.uc_stack.ss_sp = co->stack;
		co->ctx.uc_stack.ss_size = co->stack_size;
	} else {
		co->ctx.uc_stack.ss_sp = co->sched->stack;
		co->ctx.uc_stack.ss_size = co->sched->stack_size;
	}

	makecontext(&co->ctx, (void (*)(void)) execute, 1, (void*)co);
	CLRBIT(co->status, COROUTINE_STATUS_NEW);
	return;
}

int
dyco_asymcoro_create(proc_coroutine func, void *arg)
{
	dyco_coroutine *co = newcoro();
	if (co == NULL) {
		printf("Failed to allocate memory for new coroutine\n");
		return -2;
	}

	co->func = func;
	co->arg = arg;
	SETBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC);

	dyco_schedule *sched = get_sched();
	co->sched = sched;
	// co->cid = sched->_cid_gen++;
	sched->_cid_gen = (sched->_cid_gen + 1) & 0xffffff;
	co->cid = ((sched->sched_id & 0xff) << 24) | sched->_cid_gen;
	int ret = htable_insert(&sched->cid_co_map, co->cid, co);
	assert(ret >= 0);
	++sched->coro_count;
	return co->cid;
	
	return 0;
}


int
dyco_asymcororesume(int cid)
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co_prev = sched->curr_thread;
	dyco_coroutine *co_next = htable_find(&sched->cid_co_map, cid);
	if ((co_next == NULL) || (!TESTBIT(co_next->status, COROUTINE_FLAGS_ASYMMETRIC))) {
		return -1;
	}
	if (TESTBIT(co_next->status, COROUTINE_STATUS_EXITED)) {
		return 0;
	}

	if (co_prev != NULL && !TESTBIT(co_next->status, COROUTINE_FLAGS_OWNSTACK)) {
		dyco_asymcoro_setStack(cid, NULL, DYCO_MAX_STACKSIZE);
	}

	if (TESTBIT(co_next->status, COROUTINE_STATUS_NEW)) {
		init_coro(co_next);
	} else {
		loadstk(co_next);
	}

	++co_next->sched_count;
	
	sched->curr_thread = co_next;
	SETBIT(co_next->status, COROUTINE_STATUS_RUNNING);
	swapcontext(&co_next->ret, &co_next->ctx);
	sched->curr_thread = co_prev;

	if (TESTBIT(co_next->status, COROUTINE_STATUS_EXITED)) {
		return 0;
	}
	return cid;
}


void
dyco_asymcoroyield()
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return;
	}
	dyco_coroutine *co = sched->curr_thread;
	assert(co != NULL);

	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED) == 0) {
		savestk(co);
	}
	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	swapcontext(&co->ctx, &co->ret);
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
}


void dyco_asymcoro_free(int cid)
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return;
	}
	dyco_coroutine *co = htable_find(&sched->cid_co_map, cid);
	if ((co == NULL) || (!TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC))) {
		assert(0);
		return;
	}

	--co->sched->coro_count;
	htable_delete(&co->sched->cid_co_map, co->cid, NULL);

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

