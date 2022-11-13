#include "dyco/dyco_coroutine.h"

static void
hdc_notify(dyco_channel* chan, int fd)
{
	eventfd_write(fd, (eventfd_t)(chan->status));
	return;
}

static half_duplex_channel_status
hdc_wait(dyco_channel* chan, int fd, int timeout)
{
	if (timeout == 0) {
		return chan->status;
	}

	dyco_schedule *sched = get_sched();
	if (sched == NULL) {
		return chan->status;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return chan->status;
	}
	DYCO_MUSTNOT(TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));

	schedule_sched_waitR(co, fd);
	schedule_sched_sleep(co, timeout);
	yield(co);
	schedule_cancel_sleep(co);
	schedule_cancel_wait(co, fd);

	eventfd_t count;
	int ret;
    	ret = eventfd_read(fd, &count);
	if (ret == 0) {
		return (half_duplex_channel_status)(count);
	}	
	else {
		return HDC_STATUS_NOP;
	}
		
}


dyco_channel*
dyco_channel_create(size_t size)
{
	dyco_channel *chan = (dyco_channel*)malloc(sizeof(dyco_channel));
	if (chan == NULL)
		return NULL;
	chan->r_notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	DYCO_MUSTNOT(chan->r_notifyfd == -1);

	chan->w_notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	DYCO_MUSTNOT(chan->w_notifyfd == -1);

	chan->maxsize = size > 0 ? size : DYCO_DEFAULT_CHANNELSIZE;
	chan->msg = malloc(chan->maxsize);
	if (chan->msg == NULL) {
		close(chan->r_notifyfd);
		close(chan->w_notifyfd);
		free(chan);
		return NULL;
	}
	chan->msglen = 0;
	chan->status = HDC_STATUS_EMPTY;	
	return chan;
}


void
dyco_channel_destroy(dyco_channel **chan)
{
	dyco_channel *channel = *chan;
	if ((channel == NULL) || (channel->status = HDC_STATUS_WANTCLOSE)) {
		return;
	}

	if (channel->status == HDC_STATUS_WANTREAD) {
		channel->status = HDC_STATUS_WANTCLOSE;
		hdc_notify(channel, channel->r_notifyfd);
		hdc_wait(channel, channel->w_notifyfd, -1);
	}
	else if (channel->status == HDC_STATUS_WANTWRITE) {
		channel->status = HDC_STATUS_WANTCLOSE;
		hdc_notify(channel, channel->w_notifyfd);
		hdc_wait(channel, channel->r_notifyfd, -1);
	}

	close(channel->r_notifyfd);
	close(channel->w_notifyfd);
	free(channel->msg);
	free(channel);
	*chan = NULL;
	return;
}


ssize_t
dyco_channel_send(dyco_channel *channel, void *buf, size_t size, int timeout)
{
	if ((channel == NULL) || (size > channel->maxsize) || (size == 0)) 
		return -2;
	
	int ret = -2;
	switch (channel->status) {
		case HDC_STATUS_EMPTY:
			channel->msglen = size;
			memcpy(channel->msg, buf, size);

			channel->status = HDC_STATUS_FULL;
			ret = size;
			break;

		case HDC_STATUS_FULL:
			channel->status = HDC_STATUS_WANTWRITE;

			hdc_wait(channel, channel->w_notifyfd, timeout);

			switch(channel->status) {
				case HDC_STATUS_EMPTY:
					channel->msglen = size;
					memcpy(channel->msg, buf, size);
					channel->status = HDC_STATUS_FULL;
					ret = size;
					break;
				case HDC_STATUS_FULL:
					DYCO_WARNIF(1, "channel is full.");
					break;
				case HDC_STATUS_WANTREAD:
					channel->msglen = size;
					memcpy(channel->msg, buf, size);
					channel->status = HDC_STATUS_FULL;
					hdc_notify(channel, channel->r_notifyfd);
					ret = size;
					break;
				case HDC_STATUS_WANTWRITE:
					channel->status = HDC_STATUS_FULL;
					ret = 0;
					break;
				case HDC_STATUS_WANTCLOSE:
					channel->status = HDC_STATUS_CANCLOSE;
					hdc_notify(channel, channel->r_notifyfd);
				default:
					ret = -1;
					break;
			}
			break;

		case HDC_STATUS_WANTREAD:
			channel->msglen = size;
			memcpy(channel->msg, buf, size);

			channel->status = HDC_STATUS_FULL;
			hdc_notify(channel, channel->r_notifyfd);

			ret = size;
			break;

		case HDC_STATUS_WANTWRITE:
			break;

		case HDC_STATUS_WANTCLOSE:
			ret = -1;
			break;

		default: break;
	}
	return ret;
}


ssize_t
dyco_channel_recv(dyco_channel *channel, void *buf, size_t maxsize, int timeout)
{
	if ((channel == NULL) || (maxsize < channel->maxsize))
		return -2;

	int ret = -2;
	switch (channel->status) {
		case HDC_STATUS_EMPTY:
			channel->status = HDC_STATUS_WANTREAD;

			hdc_wait(channel,  channel->r_notifyfd, timeout);

			switch(channel->status) {
				case HDC_STATUS_EMPTY:
					DYCO_WARNIF(1, "channel is empty.");
					break;
				case HDC_STATUS_FULL:
					ret = channel->msglen;
					memcpy(buf, channel->msg, ret);
					channel->status = HDC_STATUS_EMPTY;
					break;
				case HDC_STATUS_WANTREAD:
					channel->status = HDC_STATUS_EMPTY;
					ret = 0;
					break;
				case HDC_STATUS_WANTWRITE:
					ret = channel->msglen;
					memcpy(buf, channel->msg, ret);
					channel->status = HDC_STATUS_EMPTY;
					hdc_notify(channel, channel->w_notifyfd);
					break;
				case HDC_STATUS_WANTCLOSE:
					channel->status = HDC_STATUS_CANCLOSE;
					hdc_notify(channel, channel->w_notifyfd);
				default:
					ret = -1;
					break;
			}
			break;

		case HDC_STATUS_FULL:
			ret = channel->msglen;
			memcpy(buf, channel->msg, ret);

			channel->status = HDC_STATUS_EMPTY;
			break;

		case HDC_STATUS_WANTREAD:
			break;
			
		case HDC_STATUS_WANTWRITE:
			ret = channel->msglen;
			memcpy(buf, channel->msg, ret);

			channel->status = HDC_STATUS_EMPTY;
			hdc_notify(channel, channel->w_notifyfd);
			break;

		case HDC_STATUS_WANTCLOSE:
			ret = -1;
			break;

		default: break;
	}
	return ret;
}
