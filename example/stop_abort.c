#include "dyco/dyco_coroutine.h"

void corofun(void *arg)
{
	int sleeptime = *(int*)arg;
	int counter = 0;
	int randadder = 0;
	int iter = 10;
	int i;
	for (i = 0; i < iter; i++) {
		randadder = rand() % 100;
		printf("I am coro %d. I will calculate %d + %d\n", dyco_coroutine_coroID(), counter, randadder);
		dyco_coroutine_sleep(sleeptime);
		counter += randadder;
		printf("I am coro %d. My answer is %d\n", dyco_coroutine_coroID(), counter);
	}
	return;
}

void foobar(void *arg)
{
	int actiontime = *(int*)arg;
	printf("I am coro %d. I will sleep %d then stop the schedualer\n", dyco_coroutine_coroID(), actiontime);
	dyco_coroutine_sleep(actiontime);
	dyco_schedcall_stop();

	printf("I am coro %d. I will sleep %d then abort the schedualer\n", dyco_coroutine_coroID(), actiontime);
	dyco_coroutine_sleep(actiontime);
	dyco_schedcall_abort();

	return;
}

int main()
{
	int i, ret;
	int sleeptime = 1000;
	int actiontime = 3600;

	for (i = 0; i < 4; i++) {
		dyco_coroutine_create(corofun, &sleeptime);
	}
	dyco_coroutine_create(foobar, &actiontime);
	printf("[+] scheduler start running.\n");
	ret = dyco_schedule_run();
	printf("[-] scheduler stopped. ret = %d\n", ret);

	sleep(3);
	printf("[+] scheduler restart.\n");
	ret = dyco_schedule_run();
	printf("[-] scheduler stopped. ret = %d\n", ret);

	sleep(3);
	printf("[+] scheduler restart.\n");
	ret = dyco_schedule_run();
	printf("[-] scheduler stopped. ret = %d\n", ret);

	return 0;
}
