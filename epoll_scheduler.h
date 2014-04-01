/*
 * netcoro.h
 *
 *  Created on: 2012-11-17
 *      Author: yelu
 */

#ifndef EPOLL_CORO_H_
#define EPOLL_CORO_H_

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

class io_coro_t;

#define MAX_EVENTS 64

typedef io_coro_t* (*IO_CORO_MAKER)();

/**
 * @brief a corontine scheduler implenmentation for network io(use epoll).
 */
class epoll_coro_t:public coro_t
{
public:

	typedef boost::function<void(int, epoll_coro_t*)> NEW_CONN_CALLBACK;

	epoll_coro_t(int listen_port);
	virtual ~epoll_coro_t();

	int run(NEW_CONN_CALLBACK& new_conn_callback);

	int monitor_read_once(int fd, io_coro_t* io_coro);

	int monitor_write_once(int fd, io_coro_t* io_coro);

	void callback(){}

private:

	static int _create_and_bind(int port);
	static int _make_socket_non_blocking(int sfd);

	void _run_event_loop();
	int _accept_new_conn() throw();
	int _handle_new_conn(int) throw();

	int _listen_fd;
	int _listen_port;
	int _epollfd;
	struct epoll_event* _events;	/* Buffer where events are returned */
	std::map<int, io_coro_t*> _io_coro_map;
	NEW_CONN_CALLBACK _new_conn_callback;
};

#endif /* EPOLL_CORO_H_ */
