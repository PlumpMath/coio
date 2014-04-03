/*
 * netcoro.cpp
 *
 *  Created on: 2012-11-17
 *      Author: yelu
 */

#include "epoll_scheduler.h"
#include "coro.h"

epoll_scheduler::epoll_scheduler()
{
	_epollfd = -1;
	_events = new epoll_event[MAX_EVENTS];
}

epoll_scheduler::~epoll_scheduler()
{
	LOG(DEBUG, "enter deconstructor.");
	delete []_events;
	_events = NULL;
	LOG(DEBUG, "delete _events.");
	std::map<int, coro*>::iterator ite = _coro_map.begin();
	for (; ite != _coro_map.end(); ++ite)
	{
		delete ite->second;
		ite->second = NULL;
	}
	LOG(DEBUG, "delete _io_coro_map.");
}

int epoll_scheduler::monitor_read_once(int fd, coro* io_coro)
{
	if(-1 == _epollfd || NULL == io_coro || fd < 0)
	{
		LOG_WARNING("wrong params[fd:%d][io_coro_t:%p].", fd, io_coro);
		return -1;
	}
	struct epoll_event event;
	event.data.fd = fd;
	// monitor read event on socket.
	event.events = EPOLLIN | EPOLLONESHOT;
	if (-1 == epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &event))
	{
		LOG_WARNING("epoll_ctl failed[fd:%d][err %d:%s].", _epollfd, errno,
				strerror(errno));
		return -1;
	}
	// add <fd, io_coro_t*> pair to map.
	_coro_map[fd] = io_coro;
	return 0;
}

int epoll_scheduler::monitor_read_continously(int fd, coro* io_coro)
{
	if(-1 == _epollfd || NULL == io_coro || fd < 0)
	{
		LOG_WARNING("wrong params[fd:%d][io_coro_t:%p].", fd, io_coro);
		return -1;
	}
	struct epoll_event event;
	event.data.fd = fd;
	// monitor read event on socket.
	event.events = EPOLLIN;
	if (-1 == epoll_ctl(_epollfd, EPOLL_CTL_ADD, fd, &event))
	{
		LOG_WARNING("epoll_ctl failed[fd:%d][err %d:%s].", _epollfd, errno,
				strerror(errno));
		return -1;
	}
	// add <fd, io_coro_t*> pair to map.
	_coro_map[fd] = io_coro;
	return 0;
}

int epoll_scheduler::monitor_write_once(int fd, coro* io_coro)
{
	if(-1 == _epollfd || NULL == io_coro || fd < 0)
	{
		LOG_WARNING("wrong params[fd:%d][io_coro_t:%p].", fd, io_coro);
		return -1;
	}
	struct epoll_event event;
	event.data.fd = fd;
	// monite read event on socket.
	event.events = EPOLLOUT | EPOLLONESHOT;
	if (-1 == epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &event))
	{
		LOG_WARNING("epoll_ctl failed[fd:%d][err %d:%s].", errno, _epollfd,
				strerror(errno));
		return -1;
	}
	// add <fd, io_coro_t*> pair to map.
	_coro_map[fd] = io_coro;
	return 0;
}

int epoll_scheduler::make_fd_non_blocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (-1 == flags)
	{
		LOG(WARNING, "fcntl get flag failed[err %d:%s].", errno, strerror(errno));
		return -1;
	}

	flags |= O_NONBLOCK;
	if (-1 == fcntl(fd, F_SETFL, flags))
	{
		LOG(WARNING, "fcntl set flag O_NONBLOCK failed[err %d:%s].", errno,
				strerror(errno));
		return -1;
	}
	return 0;
}

void epoll_scheduler::run()
{
	for (;;)
	{
		LOG_DEBUG("start epoll_wait.");
		int nfds = ::epoll_wait(_epollfd, _events, MAX_EVENTS, -1);
		LOG_DEBUG("epoll_wait returns[nfds:%d]", nfds);
		if (-1 == nfds)
		{
			LOG_WARNING("epoll_wait error[err %d:%s].", errno,
					strerror(errno));
			return;
		}
		for (int i = 0; i < nfds; i++)
		{
			int fd = _events[i].data.fd;
			if ((_events[i].events & EPOLLERR) || (_events[i].events & EPOLLHUP))
			{
				//TODO:tell io_coro about the err.
				/* An error has occured on this fd, or the socket is not
				 ready for reading (why were we notified then?) */
				LOG_WARNING("epoll error with[fd:%d].", fd);
				close(fd);
				continue;
			}
			else
			{
				// a read or write event happens, switch corontine to handle it.
				coro* io_coro = _coro_map[fd];
				LOG_DEBUG("switch coroutine[fd:%d][io_coro:%p]", fd, io_coro);
				switch_to(*io_coro);
			}
		}
	}
}
