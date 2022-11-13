#include "dyco/dyco_coroutine.h"
extern void savestk(dyco_coroutine *co);

typedef struct {
	int		__how;
	sigset_t	*__set;
	sigset_t	*__oset;
} sigprocmask_args;

int schedule_callexec(dyco_schedule *sched)
{
	int shouldresume = 1;
	sigprocmask_args *sargs;
	dyco_schedcall *call = &sched->schedcall;
	switch (call->callnum) {
		case CALLNUM_SIGPROCMASK:
			sargs = call->arg;
			call->ret = sigprocmask(sargs->__how, sargs->__set, sargs->__oset);
			shouldresume = 1;
			break;
		case CALLNUM_SCHED_STOP:
			schedule_stop(sched);
			call->ret = 0;
			shouldresume = 0;
			break;
		case CALLNUM_SCHED_ABORT:
			schedule_abort(sched);
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

int dyco_schedcall_sigprocmask(int how, sigset_t *set, sigset_t *oset)
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		perror("schedcall can only be used when scheduler is running.\n");
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		perror("schedcall can only be used when scheduler is running.\n");
		return -1;
	}

	DYCO_MUST(sigprocmask(SIG_BLOCK, set, NULL) == 0);

	sigprocmask_args args = {how, set, oset};
	sched->schedcall.callnum = CALLNUM_SIGPROCMASK;
	sched->schedcall.arg = &args;

	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	SETBIT(co->status, COROUTINE_STATUS_SCHEDCALL);

	swapcontext(&co->ctx, &co->sched->ctx);

	CLRBIT(co->status, COROUTINE_STATUS_SCHEDCALL);
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	
	return sched->schedcall.ret;
}


void dyco_schedcall_stop()
{
	dyco_schedule *sched = get_sched();
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

	if (TESTBIT(co->status, COROUTINE_FLAGS_URGENT))
		TAILQ_INSERT_TAIL(&sched->urgent_ready, co, ready_next);
	else
		TAILQ_INSERT_TAIL(&sched->ready, co, ready_next);

	savestk(co);
	swapcontext(&co->ctx, &co->sched->ctx);
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	CLRBIT(co->status, COROUTINE_STATUS_SCHEDCALL);
	
	return;
}


void dyco_schedcall_abort()
{
	dyco_schedule *sched = get_sched();
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
