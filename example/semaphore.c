#include "dyco_coroutine.h"

void cofunc(void *arg)
{
	void *udata;
	dyco_schedule_getUdata(&udata);
	dyco_semaphore *sem = udata;
	
	printf("[cofunc] I am coro %d. I'm going to critical section\n", dyco_coroutine_coroID());

	assert(dyco_semaphore_wait(sem, -1) == 0);

	printf("[cofunc] I am coro %d. I'm in critical section\n", dyco_coroutine_coroID());
	dyco_coroutine_sleep(1000);

	assert(dyco_semaphore_signal(sem) == 0);
	printf("[cofunc] I am coro %d. I exited critical section\n", dyco_coroutine_coroID());
	
	return;
}



int main()
{
	int i;
	for (i = 0; i < 10; i++) {
		dyco_coroutine_create(cofunc, NULL);
	}
	
	dyco_semaphore *sem = dyco_semaphore_create(3);
	dyco_schedule_setUdata(sem);

	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");

	dyco_semaphore_destroy(&sem);
	return 0;
}