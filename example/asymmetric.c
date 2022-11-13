#include "dyco/dyco_coroutine.h"

void fibo_gen(void *arg)
{
	int *times = arg;
	int f1 = 1;
	int f2 = 1;
	int f3 = 0;
	int *number = NULL;
	dyco_asymcoro_getUdata(dyco_asymcoro_coroID(), (void**)&number);
	printf("[fibo_gen] I am coro %d. I can generate fibonacci array\n", dyco_coroutine_coroID());

	if (*times <= 0) {
		return;
	}
	else if (*times == 1) {
		*number = 1;
		return;
	}
	*number = 1;

	dyco_asymcoroyield(); // yield 1
	dyco_asymcoroyield(); // yield 1

	int i = 2;
	while (i < *times) {
		f3 = f1 + f2;
		f1 = f2;
		f2 = f3;
		*number = f3;
		dyco_asymcoroyield();  // yield 2 3 5 8 ...
		++i;
	}
	*number = -1;
	return;
}

int fibogencid;
void foo(void *arg)
{
	int sleeptime = *(int*)arg;
	printf("I am coro %d. I'm about sleep %d ms\n", dyco_coroutine_coroID(), sleeptime);
	dyco_coroutine_sleep(sleeptime);
	printf("I am coro %d. Now I wake up and resume fibo_gen\n", dyco_coroutine_coroID());

	int *number = NULL;
	int ret = dyco_asymcoro_getUdata(fibogencid, (void**)&number);
	if (ret != 0) {
		return;
	}
	ret = dyco_asymcororesume(fibogencid);

	printf("[+] coro %d get: %d. ret = %d\n", dyco_coroutine_coroID(), *number, ret);
	if (ret == 0) {
		printf("second asymmetric coroutine finished.\n");
		dyco_asymcoro_free(fibogencid);
	}
	return;
}

void bar(void *arg)
{
	int sleeptime = *(int*)arg;
	int i = 0;
	int *times = (int*)malloc(sizeof(int));
	*times = 10;
	int cid = dyco_asymcoro_create(fibo_gen, times);
	assert(cid > 0);

	int *number = (int*)malloc(sizeof(int));
	*number = 0;
	dyco_asymcoro_setUdata(cid, number);
	
	dyco_asymcoro_setStack(cid, NULL, 4096);
	while (dyco_asymcororesume(cid)) {
		printf("[+] coro %d get %d\n", dyco_coroutine_coroID(), *number);
		dyco_coroutine_sleep(sleeptime);
	}
	dyco_asymcoro_free(cid);
}

int main()
{
	printf("\tA. create and resume asymmetric coroutines in main thread.\n");
	int i = 0;
	int *times = (int*)malloc(sizeof(int));
	*times = 30;
	int cid = dyco_asymcoro_create(fibo_gen, times);
	assert(cid > 0);

	int *number = (int*)malloc(sizeof(int));
	*number = 0;
	dyco_asymcoro_setUdata(cid, number);
	
	dyco_asymcoro_setStack(cid, NULL, 4096);
	while (dyco_asymcororesume(cid)) {
		printf("%dth generate: %d\n", ++i, *number);
	}
	dyco_asymcoro_free(cid);
	printf("first asymmetric coroutine finished.\n");

	printf("\tB. create asymmetric coroutines in main thread and resume in coroutines.\n");
	*times = 4;
	fibogencid = dyco_asymcoro_create(fibo_gen, times);
	*number = -1;
	dyco_asymcoro_setUdata(fibogencid, number);
	
	int sleeptime[8];
	for (i = 0; i < 8; i++) {
		sleeptime[i] = (i + 1) * 1000;
		dyco_coroutine_create(foo, sleeptime + i);
	}
	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");
	
	printf("\tC. create and resume asymmetric coroutines in coroutines.\n");
	int t1 = 900, t2 = 500;
	dyco_coroutine_create(bar, &t1);
	dyco_coroutine_create(bar, &t2);
	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");

	return 0;
}