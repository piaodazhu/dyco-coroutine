#include "dyco_coroutine.h"

pthread_key_t global_sched_key;
static pthread_once_t sched_key_once = PTHREAD_ONCE_INIT;
extern int dyco_schedule_create(int stack_size);

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
_save_stack(dyco_coroutine *co) {
	char* top = co->sched->stack + co->sched->stack_size;
	char dummy = 0;
	assert(top - &dummy <= DYCO_MAX_STACKSIZE);
	if (co->stack_size < top - &dummy) {
		co->stack = realloc(co->stack, top - &dummy);
		assert(co->stack != NULL);
	}
	co->stack_size = top - &dummy;
	memcpy(co->stack, &dummy, co->stack_size);
}

static void
_load_stack(dyco_coroutine *co) {
	memcpy(co->sched->stack + co->sched->stack_size - co->stack_size, co->stack, co->stack_size);
}

static void 
_exec(void *lt) {
	dyco_coroutine *co = (dyco_coroutine*)lt;
	co->func(co->arg);
	SETBIT(co->status, COROUTINE_STATUS_EXITED);
	_yield(co);
}

void 
_yield(dyco_coroutine *co) {
	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED) == 0) {
		_save_stack(co);
	}

	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	swapcontext(&co->ctx, &co->sched->ctx);
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
}

int 
_resume(dyco_coroutine *co) {
	if (TESTBIT(co->status, COROUTINE_STATUS_NEW)) {
		dyco_coroutine_init(co);
	}
	else {
		_load_stack(co);
	}

	printf("-- resume coro %d --\n", co->id);
	dyco_schedule *sched = co->sched;
	++co->sched_count;
	
	sched->curr_thread = co;
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	swapcontext(&sched->ctx, &co->ctx);
	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	sched->curr_thread = NULL;

	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED)) {
		printf("** finish coro %d **\n", co->id);
		_schedule_cancel_sleep(co);
		dyco_coroutine_free(co);
		return -1;
	} 
	return 0;
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

static void 
dyco_coroutine_init(dyco_coroutine *co) {

	getcontext(&co->ctx);
	co->ctx.uc_stack.ss_sp = co->sched->stack;
	co->ctx.uc_stack.ss_size = co->sched->stack_size;

	makecontext(&co->ctx, (void (*)(void)) _exec, 1, (void*)co);

	CLRBIT(co->status, COROUTINE_STATUS_NEW);
	return;
}

int 
dyco_coroutine_create(dyco_coroutine **new_co, proc_coroutine func, void *arg) {

	assert(pthread_once(&sched_key_once, _sched_key_creator) == 0);
	dyco_schedule *sched = _get_sched();

	if (sched == NULL) {
		dyco_schedule_create(0);
		
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

	co->id = sched->_id_gen++;
	co->ownstack = 0;

	co->data = NULL;

	co->events = 0;
	co->fd = -1;
	co->epollfd = -1;
	
	*new_co = co;
	++sched->coro_count;
	TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);

	return 0;
}

void 
dyco_coroutine_free(dyco_coroutine *co) {
	if (co == NULL) return;
	--co->sched->coro_count;

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



