#include "dyco_coroutine.h"
extern void _save_stack(dyco_coroutine *co);

typedef struct {
	int		__how;
	sigset_t	*__set;
	sigset_t	*__oset;
} _sigprocmask_args;

int _schedule_callexec(dyco_schedule *__sched)
{
	int shouldresume = 1;
	_sigprocmask_args *sargs;
	dyco_schedcall *call = &__sched->schedcall;
	switch (call->callnum) {
		case CALLNUM_SIGPROCMASK:
			sargs = call->arg;
			call->ret = sigprocmask(sargs->__how, sargs->__set, sargs->__oset);
			shouldresume = 1;
			break;
		case CALLNUM_SCHED_STOP:
			_schedule_stop(__sched);
			call->ret = 0;
			shouldresume = 0;
			break;
		case CALLNUM_SCHED_ABORT:
			_schedule_abort(__sched);
			call->ret = 0;
			shouldresume = 0;
			break;
		default:
			call->ret = -1;
			shouldresume = 1;
			break;
	}
	return shouldresume;
}

int dyco_schedcall_sigprocmask(int __how, sigset_t *__set, sigset_t *__oset)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		perror("schedcall can only be used when scheduler is running.\n");
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		perror("schedcall can only be used when scheduler is running.\n");
		return -1;
	}

	sigprocmask(SIG_BLOCK, __set, NULL);

	_sigprocmask_args args = {__how, __set, __oset};
	sched->schedcall.callnum = CALLNUM_SIGPROCMASK;
	sched->schedcall.arg = &args;

	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	SETBIT(co->status, COROUTINE_STATUS_SCHEDCALL);
	printf("call start\n");
	swapcontext(&co->ctx, &co->sched->ctx);
	printf("call back\n");
	CLRBIT(co->status, COROUTINE_STATUS_SCHEDCALL);
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	
	return sched->schedcall.ret;
}


void dyco_schedcall_stop()
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		perror("schedcall can only be used when scheduler is running.\n");
		return;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		perror("schedcall can only be used when scheduler is running.\n");
		return;
	}

	sched->schedcall.callnum = CALLNUM_SCHED_STOP;

	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	SETBIT(co->status, COROUTINE_STATUS_SCHEDCALL);
	SETBIT(co->status, COROUTINE_STATUS_READY);
	TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);
	_save_stack(co);
	swapcontext(&co->ctx, &co->sched->ctx);
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	CLRBIT(co->status, COROUTINE_STATUS_SCHEDCALL);
	
	return;
}


void dyco_schedcall_abort()
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		perror("schedcall can only be used when scheduler is running.\n");
		return;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		perror("schedcall can only be used when scheduler is running.\n");
		return;
	}

	sched->schedcall.callnum = CALLNUM_SCHED_ABORT;

	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	SETBIT(co->status, COROUTINE_STATUS_SCHEDCALL);
	swapcontext(&co->ctx, &co->sched->ctx);
	CLRBIT(co->status, COROUTINE_STATUS_SCHEDCALL);
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	
	return;
}
