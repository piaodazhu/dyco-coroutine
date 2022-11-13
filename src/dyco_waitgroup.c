#ifndef _DYCO_WAITGROUP_H
#define _DYCO_WAITGROUP_H

#include "dyco/dyco_coroutine.h"

static void
_wg_notify(int notifyfd, int finished_num)
{
	DYCO_MUST(eventfd_write(notifyfd, (eventfd_t)finished_num) == 0);
	return;
}

void
_wgsublist_free(void *head)
{
	dyco_sublist *ptr = head;
	dyco_sublist *next;
	while (ptr != NULL) {
		next = ptr->next;
		close(ptr->notifyfd);
		free(ptr);
		ptr = next;
	}
	return;
}

dyco_waitgroup*
dyco_waitgroup_create(int __suggest_size)
{
	dyco_waitgroup *wg = (dyco_waitgroup*)malloc(sizeof(dyco_waitgroup));
	if (wg == NULL) 
		return NULL;
	wg->tot_size = 0;
	wg->finished = 0;

	int width = 1;
	if (__suggest_size <= 0) {
		width = DYCO_HTABLE_DEFAULTWITDH;
	} 
	else if (__suggest_size > 65536) {
		width = DYCO_HTABLE_MAXWITDH;
	}
	else {
		while ((__suggest_size >> width) != 0) ++width;
	}
	_htable_init(&wg->cid_set, width);
	_htable_init(&wg->target_sublist_map, width);
	wg->final_sublist = NULL;
	return wg;
}


void
dyco_waitgroup_destroy(dyco_waitgroup** __group)
{
	dyco_waitgroup *wg = *__group;
	if (wg == NULL) {
		return;
	}
	_wgsublist_free(wg->final_sublist);
	_htable_clear_with_freecb(&wg->target_sublist_map, _wgsublist_free);
	_htable_clear(&wg->cid_set);
	free(wg);
	*__group = NULL;
	return;
}


int
dyco_waitgroup_add(dyco_waitgroup* __group, int __cid)
{
	if (__group == NULL)
		return -1;
	int ret = _htable_insert(&__group->cid_set, __cid, NULL);
	if (ret != 1)
		return 0;
	++(__group->tot_size);
	return 1;
}


int
dyco_waitgroup_done(dyco_waitgroup* __group)
{
	if (__group == NULL)
		return -1;
	
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return -1;
	}
	int ret = _htable_contains(&__group->cid_set, co->cid);
	if (ret != 1) {
		return 0;
	}
	
	++(__group->finished);
	// notify target subscribe list
	dyco_sublist *tslist = _htable_find(&__group->target_sublist_map, __group->finished);
	if (tslist != NULL) {
		dyco_sublist *ptr = tslist;
		while (ptr != NULL) {
			_wg_notify(ptr->notifyfd, __group->finished);
			ptr = ptr->next;
		}
	}

	// notify finnal list
	if (__group->finished == __group->tot_size) {
		dyco_sublist *ptr = __group->final_sublist;
		while (ptr != NULL) {
			_wg_notify(ptr->notifyfd, __group->finished);
			ptr = ptr->next;
		}
	}

	return 1;
}


int
dyco_waitgroup_wait(dyco_waitgroup* __group, int __target, int __timeout)
{
	if (__group == NULL) {
		return -1;
	}

	if (__timeout == 0) {
		return __group->finished;
	}

	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return -1;
	}
	assert(!TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));

	eventfd_t count;
	int ret;

	if (__target <= 0) {
		// wait until all tasks finishes
		if (__group->finished == __group->tot_size)
			return __group->finished;
		
		int notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		DYCO_MUSTNOT(notifyfd == -1);

		dyco_sublist* sub = (dyco_sublist*)malloc(sizeof(dyco_sublist));
		if (sub == NULL) {
			close(notifyfd);
			return -1;
		}
		sub->notifyfd = notifyfd;
		sub->next = NULL;

		if (__group->final_sublist == NULL) {
			__group->final_sublist = sub;
		} else {
			sub->next = __group->final_sublist->next;
			__group->final_sublist->next = sub;
		}
		
		_schedule_sched_waitR(co, notifyfd);
		_schedule_sched_sleep(co, __timeout);
		_yield(co);
		_schedule_cancel_sleep(co);
		_schedule_cancel_wait(co, notifyfd);

		ret = eventfd_read(notifyfd, &count);

		dyco_sublist* pre = __group->final_sublist;
		dyco_sublist* ptr = pre->next;
		if (pre->notifyfd == notifyfd) {
			__group->final_sublist = ptr;
			close(pre->notifyfd);
			free(pre);
		} else {
			while (ptr != NULL) {
				if (ptr->notifyfd == notifyfd) {
					pre->next = ptr->next;
					close(ptr->notifyfd);
					free(ptr);
					break;
				}
				pre = ptr;
				ptr = ptr->next;
			}
		}
	} else {
		// wait until __target tasks finishes
		if (__group->finished >= __target)
			return __target;
		
		int notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		DYCO_MUSTNOT(notifyfd == -1);

		dyco_sublist* sub = (dyco_sublist*)malloc(sizeof(dyco_sublist));
		if (sub == NULL) {
			close(notifyfd);
			return -1;
		}
			
		sub->notifyfd = notifyfd;
		sub->next = NULL;

		dyco_sublist* sublist = _htable_find(&__group->target_sublist_map, __target);
		sub->next = sublist;
		_htable_insert(&__group->target_sublist_map, __target, sub);

		_schedule_sched_waitR(co, notifyfd);
		_schedule_sched_sleep(co, __timeout);
		_yield(co);
		_schedule_cancel_sleep(co);
		_schedule_cancel_wait(co, notifyfd);

		ret = eventfd_read(notifyfd, &count);

		dyco_sublist* pre = _htable_find(&__group->target_sublist_map, __target);
		dyco_sublist* ptr = pre->next;
		if (pre->notifyfd == notifyfd) {
			if (ptr != NULL)
				_htable_insert(&__group->target_sublist_map, __target, ptr);
			else	
				_htable_delete(&__group->target_sublist_map, __target, NULL);
			close(pre->notifyfd);
			free(pre);
		} else {
			while (ptr != NULL) {
				if (ptr->notifyfd == notifyfd) {
					pre->next = ptr->next;
					close(ptr->notifyfd);
					free(ptr);
					break;
				}
				pre = ptr;
				ptr = ptr->next;
			}
		}
	}
	if (ret == 0)
		return (int)(count);
	else
		return -1;
}

#endif