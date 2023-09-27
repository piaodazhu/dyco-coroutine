#include "dyco/dyco_coroutine.h"

pthread_key_t global_sched_key;
static pthread_once_t sched_key_once = PTHREAD_ONCE_INIT;

extern void dyco_coropool_return(dyco_coroutine *co);

static void 
sched_key_destructor(void *data)
{
	free(data);
}

static void 
sched_key_creator(void)
{
	int ret;
	ret = pthread_key_create(&global_sched_key, sched_key_destructor);
	assert(ret == 0);
	ret = pthread_setspecific(global_sched_key, NULL);
	assert(ret == 0);
	return;
}

static void 
execute(void *c)
{
	dyco_coroutine *co = (dyco_coroutine*)c;
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
	co->func(co->arg);
	SETBIT(co->status, COROUTINE_STATUS_EXITED);
	yield(co);
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

void
savestk(dyco_coroutine *co)
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

int
checkstk(dyco_coroutine *co)
{
	int total_size = co->sched->stack_size;
	char* top = co->sched->stack + co->sched->stack_size;
	int cur_size = 0;
	if (TESTBIT(co->status, COROUTINE_FLAGS_OWNSTACK)) {
		top = co->stack + co->stack_size;
		total_size = co->stack_size;
	}
	char dummy = 0;
	cur_size = top - &dummy;
	// printf("stacksize=%d, cosize=%d, totalsize=%d, top=%p, dummy=%p, used=%d\n", co->sched->stack_size, co->stack_size, total_size, top, &dummy, cur_size);
	if (total_size < cur_size) { // already overflow. KILL
		// printf("[FATAL] coroutine[%d] running stack overflow! (%d/%d)\n", co->cid, cur_size, total_size);
		SETBIT(co->status, COROUTINE_STATUS_KILLED);
		CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
		swapcontext(&co->ctx, &co->sched->ctx);
	} else if ((total_size - (total_size >> 3)) < cur_size) {
		// printf("[WARN] coroutine[%d] running stack almost overflow. (%d/%d)\n", co->cid, cur_size, total_size);
		return 1;
	}
	return 0;
}

void
loadstk(dyco_coroutine *co)
{
	if (TESTBIT(co->status, COROUTINE_FLAGS_OWNSTACK))
		return;
	
	memcpy(co->sched->stack + co->sched->stack_size - co->stack_size, co->stack, co->stack_size);
	return;
}

void 
yield(dyco_coroutine *co)
{
	checkstk(co); // auto check stack usage
	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED) == 0) {
		savestk(co);
	}
	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	swapcontext(&co->ctx, &co->sched->ctx);
	SETBIT(co->status, COROUTINE_STATUS_RUNNING);
}

int 
resume(dyco_coroutine *co)
{
	if (TESTBIT(co->status, COROUTINE_STATUS_NEW)) {
		init_coro(co);
	}
	else {
		loadstk(co);
	}

	dyco_schedule *sched = co->sched;
	++co->sched_count;
	
	sched->curr_thread = co;
	swapcontext(&sched->ctx, &co->ctx);
	while (TESTBIT(co->status, COROUTINE_STATUS_SCHEDCALL)) {
		if (schedule_callexec(sched)) {
			swapcontext(&sched->ctx, &co->ctx);
		} else {
			return 1;
		}
	}
	sched->curr_thread = NULL;

	if (TESTBIT(co->status, COROUTINE_STATUS_KILLED)) {
		schedule_cancel_sleep(co);
		htable_delete_with_freecb(&sched->cid_co_map, co->cid, coro_abort);
		return -1;
	}

	if (TESTBIT(co->status, COROUTINE_STATUS_EXITED)) {
		schedule_cancel_sleep(co);
		if (TESTBIT(co->status, COROUTINE_FLAGS_INCOROPOOL))
			dyco_coropool_return(co);
		else
			freecoro(co);
		
		return -1;
	}
	return 0;
}

int
waitev(int fd, unsigned int events, int timeout)
{
	// events: uint32 epoll event
	// timeout: Specifying a negative value  in  timeout  means  an infinite  timeout. Specifying  a timeout of zero causes dyco_poll_inner() to return immediately
	
	// fast return
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = (short)(events & 0xffff);

	int ret = poll_f(&pfd, 1, 0);
	if (ret > 0 || timeout == 0)
		return ret;
	
	dyco_schedule *sched = get_sched();
	DYCO_MUST(sched != NULL);
	dyco_coroutine *co = sched->curr_thread;
	DYCO_MUST(co != NULL);
	DYCO_MUSTNOT(TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));
	
	assert(!TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));
	
	schedule_sched_wait(co, fd, events);
	schedule_sched_sleep(co, timeout);
	yield(co);
	schedule_cancel_sleep(co);
	schedule_cancel_wait(co, fd);


	return poll_f(&pfd, 1, 0);
}

int 
dyco_coroutine_create(proc_coroutine func, void *arg) 
{
	dyco_coroutine *co = newcoro();
	if (co == NULL) {
		printf("Failed to allocate memory for new coroutine\n");
		return -2;
	}

	co->func = func;
	co->arg = arg;
	
	dyco_schedule *sched = get_sched();
	co->sched = sched;
	// co->cid = sched->_cid_gen++;
	sched->_cid_gen = (sched->_cid_gen + 1) & 0xffffff;
	co->cid = ((sched->sched_id & 0xff) << 24) | sched->_cid_gen;

	int ret = htable_insert(&sched->cid_co_map, co->cid, co);
	DYCO_MUST(ret >= 0);
	++sched->coro_count;
	
	TAILQ_INSERT_TAIL(&sched->ready, co, ready_next);

	if (sched->status == SCHEDULE_STATUS_DONE) {
		sched->status = SCHEDULE_STATUS_READY;
	}
	return co->cid;
}

int 
dyco_coroutine_create_urgent(proc_coroutine func, void *arg) 
{
	dyco_coroutine *co = newcoro();
	if (co == NULL) {
		printf("Failed to allocate memory for new coroutine\n");
		return -2;
	}

	co->func = func;
	co->arg = arg;
	
	dyco_schedule *sched = get_sched();
	co->sched = sched;
	// co->cid = sched->_cid_gen++;
	sched->_cid_gen = (sched->_cid_gen + 1) & 0xffffff;
	co->cid = ((sched->sched_id & 0xff) << 24) | sched->_cid_gen;

	int ret = htable_insert(&sched->cid_co_map, co->cid, co);
	DYCO_MUST(ret >= 0);
	++sched->coro_count;
	
	SETBIT(co->status, COROUTINE_FLAGS_URGENT);
	TAILQ_INSERT_TAIL(&sched->urgent_ready, co, ready_next);

	if (sched->status == SCHEDULE_STATUS_DONE) {
		sched->status = SCHEDULE_STATUS_READY;
	}
	return co->cid;
}

dyco_coroutine*
newcoro()
{
	int ret = pthread_once(&sched_key_once, sched_key_creator);
	DYCO_MUST(ret == 0);
	dyco_schedule *sched = get_sched();

	if (sched == NULL) {
		dyco_schedule_create(0, 0);
		
		sched = get_sched();
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
freecoro(dyco_coroutine *co) {
	if (co == NULL)
		return;

	--co->sched->coro_count;
	htable_delete(&co->sched->cid_co_map, co->cid, NULL);

	if (TESTBIT(co->status, COROUTINE_FLAGS_IOMULTIPLEXING)) {
		schedule_cancel_wait(co, co->epollfd);
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
	dyco_schedule *sched = get_sched();
	DYCO_MUST(sched != NULL);
	dyco_coroutine *co = sched->curr_thread;
	DYCO_MUST(co != NULL);
	DYCO_MUSTNOT(TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));

	assert(!TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));

	if (msecs == 0) {
		SETBIT(co->status, COROUTINE_STATUS_READY);
		if (TESTBIT(co->status, COROUTINE_FLAGS_URGENT))
			TAILQ_INSERT_TAIL(&sched->urgent_ready, co, ready_next);
		else
			TAILQ_INSERT_TAIL(&sched->ready, co, ready_next);
		// printf("insert to ready queue\n");
	} else {
		schedule_sched_sleep(co, msecs);
	}
	// printf("yed\n");
	yield(co);
}

void 
dyco_coroutine_abort() {
	dyco_schedule *sched = get_sched();
	DYCO_MUST(sched != NULL);
	dyco_coroutine *co = sched->curr_thread;
	DYCO_MUST(co != NULL);
	SETBIT(co->status, COROUTINE_STATUS_KILLED);
	CLRBIT(co->status, COROUTINE_STATUS_RUNNING);
	swapcontext(&co->ctx, &co->sched->ctx);
}

int
dyco_coroutine_waitRead(int fd, int timeout)
{
	return waitev(fd, EPOLLIN | EPOLLERR | EPOLLHUP, timeout); 
}


int
dyco_coroutine_waitWrite(int fd, int timeout)
{
	return waitev(fd, EPOLLOUT | EPOLLERR | EPOLLHUP, timeout);
}

int
dyco_coroutine_waitRW(int fd, int timeout)
{
	return waitev(fd, EPOLLOUT | EPOLLIN | EPOLLERR | EPOLLHUP, timeout);
}


int
dyco_coroutine_setStack(int cid, void *stackptr, size_t stacksize)
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = htable_find(&sched->cid_co_map, cid);
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
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = htable_find(&sched->cid_co_map, cid);
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
dyco_coroutine_checkStack()
{
	dyco_schedule *sched = get_sched();
	DYCO_MUST(sched != NULL);
	dyco_coroutine *co = sched->curr_thread;
	DYCO_MUST(co != NULL);
	return checkstk(co);
}

int
dyco_coroutine_coroID()
{
	dyco_schedule *sched = get_sched();
	if ((sched == NULL) || (sched->curr_thread == NULL)) {
		return -1;
	}
	return sched->curr_thread->cid;
}

int
dyco_coroutine_setUdata(int cid, void *udata)
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = htable_find(&sched->cid_co_map, cid);
	if (co == NULL) {
		return -1;
	}
	co->udata = udata;
	return 0;
}

int
dyco_coroutine_getUdata(int cid, void **udata)
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = htable_find(&sched->cid_co_map, cid);
	if (co == NULL) {
		return -1;
	}
	*udata = co->udata;
	return 0;
}

int
dyco_coroutine_setUrgent(int cid)
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = htable_find(&sched->cid_co_map, cid);
	if (co == NULL) {
		return -1;
	}
	SETBIT(co->status, COROUTINE_FLAGS_URGENT);
	return 0;
}


int
dyco_coroutine_unsetUrgent(int cid)
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = htable_find(&sched->cid_co_map, cid);
	if (co == NULL) {
		return -1;
	}
	CLRBIT(co->status, COROUTINE_FLAGS_URGENT);
	return 0;
}

int
dyco_coroutine_getSchedCount(int cid)
{
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = htable_find(&sched->cid_co_map, cid);
	if (co == NULL) {
		return -1;
	}
	return co->sched_count;
}

