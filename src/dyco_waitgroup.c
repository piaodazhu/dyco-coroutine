#ifndef DYCO_WAITGROUP_H
#define DYCO_WAITGROUP_H

#include "dyco/dyco_coroutine.h"

static void
wg_notify(int notifyfd, int finished_num)
{
	DYCO_MUST(eventfd_write(notifyfd, (eventfd_t)finished_num) == 0);
	return;
}

void
wgsublist_free(void *head)
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
dyco_waitgroup_create(int suggest_size)
{
	dyco_waitgroup *wg = (dyco_waitgroup*)malloc(sizeof(dyco_waitgroup));
	if (wg == NULL) 
		return NULL;
	wg->tot_size = 0;
	wg->finished = 0;

	int width = 1;
	if (suggest_size <= 0) {
		width = DYCO_HTABLE_DEFAULTWITDH;
	} 
	else if (suggest_size > 65536) {
		width = DYCO_HTABLE_MAXWITDH;
	}
	else {
		while ((suggest_size >> width) != 0) ++width;
	}
	htable_init(&wg->cid_set, width);
	htable_init(&wg->target_sublist_map, width);
	wg->final_sublist = NULL;
	return wg;
}


void
dyco_waitgroup_destroy(dyco_waitgroup** group)
{
	dyco_waitgroup *wg = *group;
	if (wg == NULL) {
		return;
	}
	wgsublist_free(wg->final_sublist);
	htable_clear_with_freecb(&wg->target_sublist_map, wgsublist_free);
	htable_clear(&wg->cid_set);
	free(wg);
	*group = NULL;
	return;
}


int
dyco_waitgroup_add(dyco_waitgroup* group, int cid)
{
	if (group == NULL)
		return -1;
	int ret = htable_insert(&group->cid_set, cid, NULL);
	if (ret != 1)
		return 0;
	++(group->tot_size);
	return 1;
}


int
dyco_waitgroup_done(dyco_waitgroup* group)
{
	if (group == NULL)
		return -1;
	
	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return -1;
	}
	int ret = htable_contains(&group->cid_set, co->cid);
	if (ret != 1) {
		return 0;
	}
	
	++(group->finished);
	// notify target subscribe list
	dyco_sublist *tslist = htable_find(&group->target_sublist_map, group->finished);
	if (tslist != NULL) {
		dyco_sublist *ptr = tslist;
		while (ptr != NULL) {
			wg_notify(ptr->notifyfd, group->finished);
			ptr = ptr->next;
		}
	}

	// notify finnal list
	if (group->finished == group->tot_size) {
		dyco_sublist *ptr = group->final_sublist;
		while (ptr != NULL) {
			wg_notify(ptr->notifyfd, group->finished);
			ptr = ptr->next;
		}
	}

	return 1;
}


int
dyco_waitgroup_wait(dyco_waitgroup* group, int target, int timeout)
{
	if (group == NULL) {
		return -1;
	}

	if (timeout == 0) {
		return group->finished;
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

	eventfd_t count;
	int ret;

	if (target <= 0) {
		// wait until all tasks finishes
		if (group->finished == group->tot_size)
			return group->finished;
		
		int notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		DYCO_MUSTNOT(notifyfd == -1);

		dyco_sublist* sub = (dyco_sublist*)malloc(sizeof(dyco_sublist));
		if (sub == NULL) {
			close(notifyfd);
			return -1;
		}
		sub->notifyfd = notifyfd;
		sub->next = NULL;

		if (group->final_sublist == NULL) {
			group->final_sublist = sub;
		} else {
			sub->next = group->final_sublist->next;
			group->final_sublist->next = sub;
		}
		
		schedule_sched_waitR(co, notifyfd);
		schedule_sched_sleep(co, timeout);
		yield(co);
		schedule_cancel_sleep(co);
		schedule_cancel_wait(co, notifyfd);

		ret = eventfd_read(notifyfd, &count);

		dyco_sublist* pre = group->final_sublist;
		dyco_sublist* ptr = pre->next;
		if (pre->notifyfd == notifyfd) {
			group->final_sublist = ptr;
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
		// wait until target tasks finishes
		if (group->finished >= target)
			return target;
		
		int notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		DYCO_MUSTNOT(notifyfd == -1);

		dyco_sublist* sub = (dyco_sublist*)malloc(sizeof(dyco_sublist));
		if (sub == NULL) {
			close(notifyfd);
			return -1;
		}
			
		sub->notifyfd = notifyfd;
		sub->next = NULL;

		dyco_sublist* sublist = htable_find(&group->target_sublist_map, target);
		sub->next = sublist;
		htable_insert(&group->target_sublist_map, target, sub);

		schedule_sched_waitR(co, notifyfd);
		schedule_sched_sleep(co, timeout);
		yield(co);
		schedule_cancel_sleep(co);
		schedule_cancel_wait(co, notifyfd);

		ret = eventfd_read(notifyfd, &count);

		dyco_sublist* pre = htable_find(&group->target_sublist_map, target);
		dyco_sublist* ptr = pre->next;
		if (pre->notifyfd == notifyfd) {
			if (ptr != NULL)
				htable_insert(&group->target_sublist_map, target, ptr);
			else	
				htable_delete(&group->target_sublist_map, target, NULL);
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