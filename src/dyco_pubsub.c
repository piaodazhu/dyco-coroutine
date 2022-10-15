#ifndef _DYCO_PUBSUB_CHANNEL_H
#define _DYCO_PUBSUB_CHANNEL_H

#include "dyco_coroutine.h"

static void
_psc_subnotify(dyco_pubsubchannel* pschan)
{
	dyco_sublist *ptr = pschan->sublist;
	while (ptr != NULL) {
		eventfd_write(ptr->notifyfd, (eventfd_t)(pschan->status));
		ptr = ptr->next;
	}
	pschan->ack_rem = pschan->sub_num;
	return;
}

static void
_psc_pubnotify(dyco_pubsubchannel* pschan)
{
	eventfd_write(pschan->pub_notifyfd, (eventfd_t)(pschan->status));
	return;
}

static dyco_pubsub_channel_status
_psc_subwait(dyco_pubsubchannel* pschan, int timeout)
{
	if (timeout == 0) {
		return pschan->status;
	}

	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return pschan->status;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return pschan->status;
	}

	int notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	dyco_sublist *sublist = pschan->sublist;
	dyco_sublist *subnode = (dyco_sublist*)malloc(sizeof(dyco_sublist));
	subnode->notifyfd = notifyfd;
	subnode->next = sublist;
	pschan->sublist = subnode;
	pschan->sub_num++;
	
	struct epoll_event ev;
	ev.data.fd = notifyfd;
	ev.events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET;
	epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, notifyfd, &ev);
	_schedule_sched_wait(co, notifyfd);
	_schedule_sched_sleep(co, timeout);

	_yield(co);

	_schedule_cancel_sleep(co);
	_schedule_cancel_wait(co, notifyfd);
	epoll_ctl(sched->epollfd, EPOLL_CTL_DEL, notifyfd, NULL);

	eventfd_t count;
	int ret;
	ret = eventfd_read(notifyfd, &count);

	pschan->sub_num--;
	dyco_sublist *pre = pschan->sublist;
	dyco_sublist *ptr = pre->next;
	if (pre->notifyfd == notifyfd) {
		pschan->sublist = ptr;
		close(pre->notifyfd);
		free(pre);
	} else {
		while (ptr != NULL) {
			if (ptr->notifyfd == notifyfd) {
				pre->next = ptr->next;
				close(ptr->notifyfd);
				free(pre);
				break;
			}
			pre = ptr;
			ptr = ptr->next;
		}
	}

	if (ret == 0) {
		if (--(pschan->ack_rem) == 0)
			_psc_pubnotify(pschan);

		return (dyco_pubsub_channel_status)(count);
	} else {
		return PSC_STATUS_NOP;
	}
}

static dyco_pubsub_channel_status
_psc_pubwait(dyco_pubsubchannel* pschan, int timeout)
{
	if (timeout == 0) {
		return pschan->status;
	}

	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return pschan->status;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return pschan->status;
	}

	int notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	struct epoll_event ev;
	ev.data.fd = notifyfd;
	ev.events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET;
	epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, notifyfd, &ev);
	_schedule_sched_wait(co, notifyfd);
	_schedule_sched_sleep(co, timeout);

	_yield(co);

	_schedule_cancel_sleep(co);
	_schedule_cancel_wait(co, notifyfd);
	epoll_ctl(sched->epollfd, EPOLL_CTL_DEL, notifyfd, NULL);

	eventfd_t count;
	int ret;
    	ret = eventfd_read(notifyfd, &count);

	close(notifyfd);
	if (ret == 0)
		return (dyco_pubsub_channel_status)(count);
	else
		return PSC_STATUS_NOP;
}

dyco_pubsubchannel* dyco_pubsub_create(size_t __size)
{
	dyco_pubsubchannel* pschan = (dyco_pubsubchannel*)malloc(sizeof(dyco_pubsubchannel));
	if (pschan == NULL)
		return NULL;
	pschan->maxsize = __size > 0 ? __size : DYCO_DEFAULT_CHANNELSIZE;
	pschan->msg = malloc(pschan->maxsize);
	if (pschan->msg == NULL) {
		free(pschan);
		return NULL;
	}
	pschan->msglen = 0;
	pschan->pub_notifyfd = -1;
	pschan->sub_num = 0;
	pschan->ack_rem = 0;
	pschan->sublist = NULL;
	pschan->status = PSC_STATUS_EMPTY;
	return pschan;
}


void dyco_pubsub_destroy(dyco_pubsubchannel **__pschan)
{
	dyco_pubsubchannel *pschan = *__pschan;
	if (pschan == NULL)
		return;
	if (pschan->sub_num != 0) {
		pschan->status = PSC_STATUS_CLOSE;
		_psc_subnotify(pschan);
		_psc_pubwait(pschan, -1);
	}
	free(pschan->msg);
	free(pschan);
	*__pschan = NULL;
	return;
}


ssize_t dyco_pubsub_publish(dyco_pubsubchannel *__pschan, void *__buf, size_t __size)
{
	if ((__pschan == NULL) || (__pschan->status != PSC_STATUS_EMPTY) || (__size > __pschan->maxsize) || (__size == 0))
		return -2;
	
	if (__pschan->sub_num == 0)
		return 0;
	
	__pschan->msglen = __size;
	memcpy(__pschan->msg, __buf, __size);
	__pschan->status = PSC_STATUS_TRANS;
	_psc_subnotify(__pschan);
	_psc_pubwait(__pschan, -1);
	__pschan->status = PSC_STATUS_EMPTY;
	return __size;
}


ssize_t dyco_pubsub_subscribe(dyco_pubsubchannel *__pschan, void *__buf, size_t __maxsize, int __timeout)
{
	if ((__pschan == NULL) || (__maxsize < __pschan->maxsize))
		return -2;
	
	int ret = -2;
	switch (__pschan->status) {
		case PSC_STATUS_EMPTY:
		case PSC_STATUS_TRANS:
			_psc_subwait(__pschan, __timeout);
			switch (__pschan->status) {
				case PSC_STATUS_EMPTY:
					ret = 0;
					break;
				case PSC_STATUS_TRANS:
					ret = __pschan->msglen;
					memcpy(__buf, __pschan->msg, ret);
					break;
				case PSC_STATUS_CLOSE:
					ret = -1;
					break;
				default:
					break;
			}
			break;
		case PSC_STATUS_CLOSE:
			ret = -1;
			break;
	}
	return ret;
}

#endif