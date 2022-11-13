#include "dyco/dyco_coroutine.h"

static void
psc_subnotify(dyco_pubsubchannel* pschan)
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
psc_pubnotify(dyco_pubsubchannel* pschan)
{
	DYCO_MUST(eventfd_write(pschan->pub_notifyfd, (eventfd_t)(pschan->status)) == 0);
	return;
}

static dyco_pubsub_channel_status
psc_subwait(dyco_pubsubchannel* pschan, int timeout)
{
	if (timeout == 0) {
		return pschan->status;
	}

	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return pschan->status;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return pschan->status;
	}
	assert(!TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));

	int notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	DYCO_MUSTNOT(notifyfd == -1);

	dyco_sublist *sublist = pschan->sublist;
	dyco_sublist *subnode = (dyco_sublist*)malloc(sizeof(dyco_sublist));
	DYCO_MUSTNOT(subnode == NULL);
	subnode->notifyfd = notifyfd;
	subnode->next = sublist;
	pschan->sublist = subnode;
	pschan->sub_num++;
	
	schedule_sched_waitR(co, notifyfd);
	schedule_sched_sleep(co, timeout);
	yield(co);
	schedule_cancel_sleep(co);
	schedule_cancel_wait(co, notifyfd);

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
				free(ptr);
				break;
			}
			pre = ptr;
			ptr = ptr->next;
		}
	}

	if (ret == 0) {
		if (--(pschan->ack_rem) == 0)
			// psc_pubnotify(pschan);
			return PSC_STATUS_EMPTY;

		return (dyco_pubsub_channel_status)(count);
	} else {
		return PSC_STATUS_NOP;
	}
}

static dyco_pubsub_channel_status
psc_pubwait(dyco_pubsubchannel* pschan, int timeout)
{
	if (timeout == 0) {
		return pschan->status;
	}

	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return pschan->status;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return pschan->status;
	}
	assert(!TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));

	pschan->pub_notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	DYCO_MUSTNOT(pschan->pub_notifyfd == -1);

	schedule_sched_waitR(co, pschan->pub_notifyfd);
	schedule_sched_sleep(co, timeout);
	yield(co);
	schedule_cancel_sleep(co);
	schedule_cancel_wait(co, pschan->pub_notifyfd);

	eventfd_t count;
	int ret;
    	ret = eventfd_read(pschan->pub_notifyfd, &count);

	close(pschan->pub_notifyfd);
	if (ret == 0)
		return (dyco_pubsub_channel_status)(count);
	else
		return PSC_STATUS_NOP;
}

dyco_pubsubchannel* dyco_pubsub_create(size_t size)
{
	dyco_pubsubchannel* pschan = (dyco_pubsubchannel*)malloc(sizeof(dyco_pubsubchannel));
	if (pschan == NULL)
		return NULL;
	pschan->maxsize = size > 0 ? size : DYCO_DEFAULT_CHANNELSIZE;
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


void dyco_pubsub_destroy(dyco_pubsubchannel **ps_chan)
{
	dyco_pubsubchannel *pschan = *ps_chan;
	if (pschan == NULL)
		return;
	if (pschan->sub_num != 0) {
		pschan->status = PSC_STATUS_CLOSE;
		psc_subnotify(pschan);
		psc_pubwait(pschan, -1);
	}
	free(pschan->msg);
	free(pschan);
	*ps_chan = NULL;
	return;
}


ssize_t dyco_pubsub_publish(dyco_pubsubchannel *ps_chan, void *buf, size_t size)
{
	if ((ps_chan == NULL) || (ps_chan->status != PSC_STATUS_EMPTY) || (size > ps_chan->maxsize) || (size == 0))
		return -2;
	
	if (ps_chan->sub_num == 0)
		return 0;
	
	ps_chan->msglen = size;
	memcpy(ps_chan->msg, buf, size);
	ps_chan->status = PSC_STATUS_TRANS;
	psc_subnotify(ps_chan);
	psc_pubwait(ps_chan, -1);
	ps_chan->status = PSC_STATUS_EMPTY;
	return size;
}


ssize_t dyco_pubsub_subscribe(dyco_pubsubchannel *ps_chan, void *buf, size_t maxsize, int timeout)
{
	if ((ps_chan == NULL) || (maxsize < ps_chan->maxsize))
		return -2;
	
	int ret = -2;
	dyco_pubsub_channel_status status;
	switch (ps_chan->status) {
		case PSC_STATUS_EMPTY:
		case PSC_STATUS_TRANS:
			status = psc_subwait(ps_chan, timeout);
			switch (status) {
				case PSC_STATUS_EMPTY:
					ret = ps_chan->msglen;
					memcpy(buf, ps_chan->msg, ret);
					psc_pubnotify(ps_chan);
					break;
				case PSC_STATUS_TRANS:
					ret = ps_chan->msglen;
					memcpy(buf, ps_chan->msg, ret);
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
		default:
			ret = -1;
			break;
	}
	return ret;
}
