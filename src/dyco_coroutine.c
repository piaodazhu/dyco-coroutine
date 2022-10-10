#include "dyco_coroutine.h"

pthread_key_t global_sched_key;
static pthread_once_t sched_key_once = PTHREAD_ONCE_INIT;


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

static void _exec(void *lt) {
	dyco_coroutine *co = (dyco_coroutine*)lt;
	co->func(co->arg);
	co->status |= (BIT(COROUTINE_STATUS_EXITED) | BIT(COROUTINE_STATUS_FDEOF) | BIT(COROUTINE_STATUS_DETACH));
	dyco_coroutine_yield(co);
}

extern int dyco_schedule_create(int stack_size);

void dyco_coroutine_free(dyco_coroutine *co) {
	if (co == NULL) return ;
	co->sched->spawned_coroutines --;

	if (co->stack) {
		free(co->stack);
		co->stack = NULL;
	}

	if (co) {
		free(co);
	}

}

static void dyco_coroutine_init(dyco_coroutine *co) {

	getcontext(&co->ctx);
	co->ctx.uc_stack.ss_sp = co->sched->stack;
	co->ctx.uc_stack.ss_size = co->sched->stack_size;
	// co->ctx.uc_link = &co->sched->ctx;
	// printf("TAG21\n");
	makecontext(&co->ctx, (void (*)(void)) _exec, 1, (void*)co);
	// printf("TAG22\n");

	co->status = BIT(COROUTINE_STATUS_READY);
	
}

void dyco_coroutine_yield(dyco_coroutine *co) {
	co->ops = 0;
	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED) == 0) {
		_save_stack(co);
	}
	swapcontext(&co->ctx, &co->sched->ctx);
}

int dyco_coroutine_resume(dyco_coroutine *co) {
	if (TESTBIT(co->status, COROUTINE_STATUS_NEW)) {
		dyco_coroutine_init(co);
	} 	
	else {
		_load_stack(co);
	}
	dyco_schedule *sched = _get_sched();
	sched->curr_thread = co;
	swapcontext(&sched->ctx, &co->ctx);
	sched->curr_thread = NULL;

	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED)) {
		if (TESTBIT(co->status, COROUTINE_STATUS_DETACH)) {
			printf("dyco_coroutine_resume --> \n");
			dyco_coroutine_free(co);
		}
		return -1;
	} 
	return 0;
}


void dyco_coroutine_renice(dyco_coroutine *co) {
	co->ops ++;

	if (co->ops < 5) return ;

	printf("dyco_coroutine_renice\n");
	TAILQ_INSERT_TAIL(&_get_sched()->ready, co, ready_next);
	printf("dyco_coroutine_renice 111\n");
	dyco_coroutine_yield(co);
}


void dyco_coroutine_sleep(uint64_t msecs) {
	dyco_coroutine *co = _get_sched()->curr_thread;

	if (msecs == 0) {
		TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);
		dyco_coroutine_yield(co);
	} else {
		dyco_schedule_sched_sleepdown(co, msecs);
	}
}

void dyco_coroutine_detach(void) {
	dyco_coroutine *co = _get_sched()->curr_thread;
	SETBIT(co->status, COROUTINE_STATUS_DETACH);
}

static void dyco_coroutine_sched_key_destructor(void *data) {
	free(data);
}

static void dyco_coroutine_sched_key_creator(void) {
	assert(pthread_key_create(&global_sched_key, dyco_coroutine_sched_key_destructor) == 0);
	assert(pthread_setspecific(global_sched_key, NULL) == 0);
	
	return ;
}


// coroutine --> 
// create 
//
int dyco_coroutine_create(dyco_coroutine **new_co, proc_coroutine func, void *arg) {

	assert(pthread_once(&sched_key_once, dyco_coroutine_sched_key_creator) == 0);
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

	co->stack = NULL;
	co->stack_size = 0;

	co->sched = sched;
	co->status = BIT(COROUTINE_STATUS_NEW); //
	co->id = sched->spawned_coroutines ++;
	co->func = func;
	co->fd = -1;
	co->events = 0;
	co->arg = arg;
	*new_co = co;

	TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);

	return 0;
}




