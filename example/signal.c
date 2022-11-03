#include "dyco_coroutine.h"

void foobar(void *arg)
{
	while (1) {
		printf("[foobar] I am coro %d. I'm about sleep %d ms\n", dyco_coroutine_coroID(), 1000);
		dyco_coroutine_sleep(1000);
		printf("[foobar] I am coro %d. Now I wake up\n", dyco_coroutine_coroID());
	}
	return;
}

void wait_interrupt(void *arg)
{
	int ret;
	sigset_t sigmask;
	struct signalfd_siginfo fdsi;
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGINT);
	dyco_signal_init(&sigmask);

	printf("[wait_interrupt] I am coro %d. I'm about waiting for Ctrl+C\n", dyco_coroutine_coroID());

	ret = dyco_signal_wait(&fdsi, 3000);
	if (ret < 0) {
		printf("[wait_interrupt] I am coro %d. I haven't detected Ctrl+C. I'll keep waiting...\n", dyco_coroutine_coroID());

		ret = dyco_signal_wait(&fdsi, -1);
	}
	assert(fdsi.ssi_signo == SIGINT);
	printf("[wait_interrupt] I am coro %d. Now I detected Ctrl+C\n", dyco_coroutine_coroID());
	printf("[wait_interrupt] I am coro %d. Now I'm about sleep 3s. You can't Ctrl+C me.\n", dyco_coroutine_coroID());

	dyco_coroutine_sleep(3000);

	printf("[wait_interrupt] I am coro %d. Now I wake up. I'm about waiting for Ctrl+C\n", dyco_coroutine_coroID());

	ret = dyco_signal_wait(&fdsi, -1);

	printf("[wait_interrupt] I am coro %d. Now I detected Ctrl+C\n", dyco_coroutine_coroID());

	dyco_signal_destroy();

	printf("[wait_interrupt] I am coro %d. Now I'm about sleep 7s. But you can Ctrl+C me.\n", dyco_coroutine_coroID());

	dyco_coroutine_sleep(7000);

	printf("[wait_interrupt] I am coro %d. Bye Bye.\n", dyco_coroutine_coroID());
	return;
}

void wait_child(void *arg)
{
	pid_t pid;
	int ret;
	int status;

	printf("[wait_child] I am coro %d. I'm about fork myself\n", dyco_coroutine_coroID());

	if ((pid = fork()) == 0) {
		printf("[wait_child] I am coro %d's child. I'm about sleep 1000 ms\n", dyco_coroutine_coroID());
		dyco_coroutine_sleep(1000);
		printf("[wait_child] I am coro %d's child. I wake up\n", dyco_coroutine_coroID());
		exit(2);
	} 
	else if (pid > 0) {
		printf("[wait_child] I am coro %d. I'm about wait my child...\n", dyco_coroutine_coroID());
		dyco_signal_waitchild(pid, &status, -1);
		assert(WIFEXITED(status));
	}
	else {
		perror("Fork()");
		exit(1);
	}

	printf("[wait_child] I am coro %d. I wait my child, status is %d\n", dyco_coroutine_coroID(), status);
	return;
}

int main()
{
	int sleeptime[10];
	// dyco_coroutine_create(foobar, NULL);
	dyco_coroutine_create(wait_interrupt, NULL);
	// dyco_coroutine_create(wait_child, NULL);

	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");
	return 0;
}