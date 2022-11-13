#include "dyco/dyco_coroutine.h"

static void
_hdc_notify(dyco_channel* chan, int fd)
{
	eventfd_write(fd, (eventfd_t)(chan->status));
	return;
}

static half_duplex_channel_status
_hdc_wait(dyco_channel* chan, int fd, int timeout)
{
	if (timeout == 0) {
		return chan->status;
	}

	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return chan->status;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return chan->status;
	}
	DYCO_MUSTNOT(TESTBIT(co->status, COROUTINE_FLAGS_ASYMMETRIC));

	_schedule_sched_waitR(co, fd);
	_schedule_sched_sleep(co, timeout);
	_yield(co);
	_schedule_cancel_sleep(co);
	_schedule_cancel_wait(co, fd);

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
dyco_channel_create(size_t __size)
{
	dyco_channel *chan = (dyco_channel*)malloc(sizeof(dyco_channel));
	if (chan == NULL)
		return NULL;
	chan->r_notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	DYCO_MUSTNOT(chan->r_notifyfd == -1);

	chan->w_notifyfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	DYCO_MUSTNOT(chan->w_notifyfd == -1);

	chan->maxsize = __size > 0 ? __size : DYCO_DEFAULT_CHANNELSIZE;
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
	dyco_channel *__chan = *chan;
	if ((__chan == NULL) || (__chan->status = HDC_STATUS_WANTCLOSE)) {
		return;
	}

	if (__chan->status == HDC_STATUS_WANTREAD) {
		__chan->status = HDC_STATUS_WANTCLOSE;
		_hdc_notify(__chan, __chan->r_notifyfd);
		_hdc_wait(__chan, __chan->w_notifyfd, -1);
	}
	else if (__chan->status == HDC_STATUS_WANTWRITE) {
		__chan->status = HDC_STATUS_WANTCLOSE;
		_hdc_notify(__chan, __chan->w_notifyfd);
		_hdc_wait(__chan, __chan->r_notifyfd, -1);
	}

	close(__chan->r_notifyfd);
	close(__chan->w_notifyfd);
	free(__chan->msg);
	free(__chan);
	*chan = NULL;
	return;
}


ssize_t
dyco_channel_send(dyco_channel *__chan, void *__buf, size_t __size, int __timeout)
{
	if ((__chan == NULL) || (__size > __chan->maxsize) || (__size == 0)) 
		return -2;
	
	int ret = -2;
	switch (__chan->status) {
		case HDC_STATUS_EMPTY:
			__chan->msglen = __size;
			memcpy(__chan->msg, __buf, __size);

			__chan->status = HDC_STATUS_FULL;
			ret = __size;
			break;

		case HDC_STATUS_FULL:
			__chan->status = HDC_STATUS_WANTWRITE;

			_hdc_wait(__chan, __chan->w_notifyfd, __timeout);

			switch(__chan->status) {
				case HDC_STATUS_EMPTY:
					__chan->msglen = __size;
					memcpy(__chan->msg, __buf, __size);
					__chan->status = HDC_STATUS_FULL;
					ret = __size;
					break;
				case HDC_STATUS_FULL:
					DYCO_WARNIF(1, "channel is full.");
					break;
				case HDC_STATUS_WANTREAD:
					__chan->msglen = __size;
					memcpy(__chan->msg, __buf, __size);
					__chan->status = HDC_STATUS_FULL;
					_hdc_notify(__chan, __chan->r_notifyfd);
					ret = __size;
					break;
				case HDC_STATUS_WANTWRITE:
					__chan->status = HDC_STATUS_FULL;
					ret = 0;
					break;
				case HDC_STATUS_WANTCLOSE:
					__chan->status = HDC_STATUS_CANCLOSE;
					_hdc_notify(__chan, __chan->r_notifyfd);
				default:
					ret = -1;
					break;
			}
			break;

		case HDC_STATUS_WANTREAD:
			__chan->msglen = __size;
			memcpy(__chan->msg, __buf, __size);

			__chan->status = HDC_STATUS_FULL;
			_hdc_notify(__chan, __chan->r_notifyfd);

			ret = __size;
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
dyco_channel_recv(dyco_channel *__chan, void *__buf, size_t __maxsize, int __timeout)
{
	if ((__chan == NULL) || (__maxsize < __chan->maxsize))
		return -2;

	int ret = -2;
	switch (__chan->status) {
		case HDC_STATUS_EMPTY:
			__chan->status = HDC_STATUS_WANTREAD;

			_hdc_wait(__chan,  __chan->r_notifyfd, __timeout);

			switch(__chan->status) {
				case HDC_STATUS_EMPTY:
					DYCO_WARNIF(1, "channel is empty.");
					break;
				case HDC_STATUS_FULL:
					ret = __chan->msglen;
					memcpy(__buf, __chan->msg, ret);
					__chan->status = HDC_STATUS_EMPTY;
					break;
				case HDC_STATUS_WANTREAD:
					__chan->status = HDC_STATUS_EMPTY;
					ret = 0;
					break;
				case HDC_STATUS_WANTWRITE:
					ret = __chan->msglen;
					memcpy(__buf, __chan->msg, ret);
					__chan->status = HDC_STATUS_EMPTY;
					_hdc_notify(__chan, __chan->w_notifyfd);
					break;
				case HDC_STATUS_WANTCLOSE:
					__chan->status = HDC_STATUS_CANCLOSE;
					_hdc_notify(__chan, __chan->w_notifyfd);
				default:
					ret = -1;
					break;
			}
			break;

		case HDC_STATUS_FULL:
			ret = __chan->msglen;
			memcpy(__buf, __chan->msg, ret);

			__chan->status = HDC_STATUS_EMPTY;
			break;

		case HDC_STATUS_WANTREAD:
			break;
			
		case HDC_STATUS_WANTWRITE:
			ret = __chan->msglen;
			memcpy(__buf, __chan->msg, ret);

			__chan->status = HDC_STATUS_EMPTY;
			_hdc_notify(__chan, __chan->w_notifyfd);
			break;

		case HDC_STATUS_WANTCLOSE:
			ret = -1;
			break;

		default: break;
	}
	return ret;
}
