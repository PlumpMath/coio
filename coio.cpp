/*
 * coio.cpp
 *
 *  Created on: Apr 3, 2014
 *      Author: luye
 */

#include <errno.h>
#include <unistd.h>
#include <cassert>
#include "epoll_scheduler.h"
#include "coio.h"
#include "log.h"

coio::coio(epoll_scheduler* es)
{
	LOG_DEBUG("enter coio constructor.");
	_es = es;
	assert(NULL != _es);
}

coio::~coio()
{
	LOG(DEBUG, "enter deconstructor.");
}

int coio::run()
{
	// this functio is called in epoll coroutine, switch from epoll coro to
	// io coro to enter io coro callback.
	LOG_DEBUG("[this:%p][epoll_coro_t:%p]", this, _es);
	_es->switch_to(*this);
}

int coio::write(fd_t& fd, char* buf, uint64_t len)
{
	if(NULL == buf || len <= 0)
	{
		LOG_WARNING("wrong params.");
		return -1;
	}
	uint64_t cnt = 0;
	int write_fd = fd.get_fd();
	while (cnt < len)
	{
		_es->monitor_write_once(write_fd, this);
		switch_to(*_es);
		ssize_t count = ::write(write_fd, buf + cnt, len - cnt);
		LOG_DEBUG("[_epoll_coro:%p][_fd:%d][write:%d]", _es, write_fd, count);
		if (count <= 0)
		{
			LOG_WARNING("socket write err[err %d:%s].", errno, strerror(errno));
			return -1;
		}
		cnt += count;
	}
	return 0;
}

int coio::read(fd_t& fd, char* buf, uint64_t len)
{
	if(NULL == buf || len <= 0)
	{
		LOG_WARNING("wrong params.");
		return -1;
	}
	uint64_t cnt = 0;
	int read_fd = fd.get_fd();
	while (cnt < len)
	{
		_es->monitor_read_once(read_fd, this);
		switch_to(*_es);
		ssize_t count = ::read(read_fd, buf + cnt, len - cnt);
		LOG_DEBUG("[_epoll_coro:%p][_fd:%d][read:%d]", _epoll_coro, read_fd, count);
		if (count <= 0)
		{
			LOG_WARNING("socket read err[err %d:%s].", errno, strerror(errno));
			return -1;
		}
		cnt += count;
	}
	return 0;
}


