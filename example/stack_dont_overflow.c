#include "dyco/dyco_coroutine.h"

static int slptime;

void otherfunc(void *arg)
{
	while (1) {
		printf("I am coro %d. I'm about sleep %d ms\n", dyco_coroutine_coroID(), 1000);
		dyco_coroutine_sleep(1000);
		printf("I am coro %d. Now I wake up\n", dyco_coroutine_coroID());
	}
	return;
}

int cal(int num) {
	if (num == 1 || num == 2) {
		return num;
	}
	char buf[1000] = "foo bar just use memory";
	int n1, n2, ret;
	n1 = num - 1;
	n2 = num - 2;
	int v1, v2;

	// Here we check stack usage: if stack usage is > 7/8, we abort this coroutine!
	if (dyco_coroutine_checkStack() != 0) {
		printf("almost overflow! Abort...\n");
		dyco_coroutine_abort();
	}
	dyco_coroutine_sleep(slptime);
	v1 = cal(n1);
	v2 = cal(n2);
	ret = v1 + v2;
	printf("%s:%d\n", buf, ret);
	return ret;
}

void fibo(void *arg) {
	// calculate fibonacci (a task that need many stack space)
	int num = *(int*)arg;
	cal(num);
}

int main()
{
	slptime = 10;
	int fn = 100;
	dyco_coroutine_create(otherfunc, NULL);
	dyco_coroutine_create(fibo, &fn);
	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");
	return 0;
}