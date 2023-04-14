#include "dyco/dyco_coroutine.h"

#define DYCO_SEM_VALUE_MAX 0x7fffffff

void sem_notify(int fd, eventfd_t data)
{
	DYCO_MUST(eventfd_write(fd, data) == 0);
}

dyco_semaphore* dyco_semaphore_create(size_t value)
{
	if (value > DYCO_SEM_VALUE_MAX) return NULL;
	dyco_semaphore *sem = (dyco_semaphore*)malloc(sizeof(dyco_semaphore));
	if (sem == NULL)
		return NULL;
	sem->semval = value;
	sem->wqueue = NULL;
	sem->wtail = NULL;
	return sem;
}


void dyco_semaphore_destroy(dyco_semaphore **__sem)
{
	dyco_semaphore* sem = *__sem;
	if (sem == NULL) return;
	dyco_semwait_queue *pre = sem->wqueue;
	dyco_semwait_queue *ptr;
	while (pre != NULL)
	{
		ptr = pre->next;
		sem_notify(pre->notifyfd, 64);
		pre = ptr;
	}
	free(sem);
	*__sem = NULL;
	return;
}


int dyco_semaphore_wait(dyco_semaphore *sem, int timeout)
{
	if (sem == NULL)
		return -1;
	if (sem->semval > 0) {
		--sem->semval;
		return 0;
	}

	if (timeout == 0) {
		if (sem->semval > 0) {
			--sem->semval;
			return 0;
		} else {
			return -1;
		}
	}

	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return -1;
	}
	assert(!TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));

	int notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	DYCO_MUSTNOT(notifyfd == -1);

	dyco_semwait_node *wnode = (dyco_semwait_node*)malloc(sizeof(dyco_semwait_node));
	if (wnode == NULL) {
		return -1;
	}
	wnode->notifyfd = notifyfd;
	wnode->next = NULL;

	if (sem->wqueue == NULL) {
		sem->wqueue = wnode;
		sem->wtail = wnode;
	} else {
		sem->wtail->next = wnode;
		sem->wtail = wnode;
	}


	schedule_sched_waitR(co, notifyfd);
	schedule_sched_sleep(co, timeout);
	yield(co);
	schedule_cancel_sleep(co);
	schedule_cancel_wait(co, notifyfd);

	eventfd_t count;
	int ret;
	ret = eventfd_read(notifyfd, &count);
	close(notifyfd);
	return ret == 0 && count == 1? 0 : -1;
}


int dyco_semaphore_signal(dyco_semaphore *sem)
{
	if (sem == NULL)
		return -1;

	if (sem->semval == DYCO_SEM_VALUE_MAX) {
		return -1;
	}

	if (sem->wqueue == NULL) {
		++sem->semval;
		return 0;
	}

	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return -1;
	}

	dyco_semwait_node *head = sem->wqueue;
	if (sem->wtail == sem->wqueue) {
		sem->wqueue = NULL;
		sem->wtail = NULL;
	} else {
		sem->wqueue = head->next;
	}

	sem_notify(head->notifyfd, 1);
	free(head);
	return 0;
}
