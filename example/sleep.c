#include "dyco/dyco_coroutine.h"

void corofun(void *arg)
{
	int sleeptime = *(int*)arg;
	printf("I am coro %d. I'm about sleep %d ms\n", dyco_coroutine_coroID(), sleeptime);
	dyco_coroutine_sleep(sleeptime);
	printf("I am coro %d. Now I wake up\n", dyco_coroutine_coroID());
	return;
}

int main()
{
	int i;
	int sleeptime[10];
	for (i = 0; i < 10; i++) {
		sleeptime[i] = 1000 * i;
		dyco_coroutine_create(corofun, sleeptime + i);
	}
	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");
	return 0;
}