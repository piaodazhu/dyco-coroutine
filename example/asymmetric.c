#include "dyco_coroutine.h"

void fibo_gen(void *arg)
{
	int *times = arg;
	int f1 = 1;
	int f2 = 1;
	int f3 = 0;
	int i = 0;
	int *number = NULL;
	while (i < *times) {
		dyco_asymcoro_getUdata(dyco_asymcoro_coroID(), (void **)&number);
		*number = f1;
		dyco_asymcoro_yield();
		f3 = f1 + f2;
		f1 = f2;
		f2 = f3;
		++i;
	}
	return;
}

int main()
{
	int t = 10;
	int cid = dyco_asymcoro_create(fibo_gen, &t);

	int *number = malloc(sizeof(int));
	*number = 0;
	dyco_asymcoro_setUdata(cid, number);
	assert(cid > 0);
	while (dyco_asymcoro_resume(cid)) {
		printf("%d\n", *number);
		sleep(1);
	}
	dyco_asymcoro_free(cid);
	return 0;
}