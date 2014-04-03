/*
 * epoll_scheduler.h
 *
 *  Created on: 2012-11-17
 *      Author: yelu
 */

#ifndef EPOLL_SCHEDULER_H_
#define EPOLL_SCHEDULER_H_

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <errno.h>
#include <map>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include "log.h"
#include "coro.h"

class coro;

#define MAX_EVENTS 64

typedef coro* (*CORO_MAKER)();

/**
 * @brief a corontine scheduler implenmentation for network io(use epoll).
 */
class epoll_scheduler:public coro
{
public:

	typedef boost::function<void(int, epoll_scheduler*)> NEW_CONN_CALLBACK;

	epoll_scheduler();
	virtual ~epoll_scheduler();

	int monitor_read_once(int fd, coro* io_coro);

	int monitor_read_continously(int fd, coro* io_coro);

	int monitor_write_once(int fd, coro* io_coro);

	void callback(){}

	void run();

	static int make_fd_non_blocking(int fd);

private:

	int _epollfd;
	struct epoll_event* _events;	/* Buffer where events are returned */
	std::map<int, coro*> _coro_map;
};

#endif /* EPOLL_CORO_H_ */
