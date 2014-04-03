/*
 * netcoro.cpp
 *
 *  Created on: 2012-11-17
 *      Author: yelu
 */

#include "iocoro/epoll_coro.h"
#include "iocoro/io_coro.h"

server::server(int listen_port)
{
	_listen_fd = -1;
	_listen_port = listen_port;
}

server::~server()
{
	LOG(DEBUG, "enter deconstructor.");
}

int server::_create_and_bind(int port)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;	/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;	/* tcp socket */
	hints.ai_flags = AI_PASSIVE;	/* For wildcard IP address */
	hints.ai_protocol = 0;          /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	char port_str[16];
	snprintf(port_str, sizeof(port_str), "%d", port);
	port_str[sizeof(port_str) - 1] = '\0';
	LOG(DEBUG, "will use [port:%s] to bind.", port_str);
	struct addrinfo* result;
	int ret = getaddrinfo(NULL, port_str, &hints, &result);
	if (0 != ret)
	{
		LOG(WARNING, "getaddrinfo error.[gai_strerror:%s].", gai_strerror(ret));
		return -1;
	}

	/* getaddrinfo() returns a list of address structures. Try each address
	 * until we successfully bind(). If socket() (or bind()) fails, we
	 * (close the socket and) try the next address. */
	int sfd = -1;
	int cnt = 0;
	struct addrinfo* rp;
	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		cnt++;
		sfd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (-1 == sfd)
		{
			LOG(DEBUG, "create socket failed[err %d:%s].", errno, strerror(errno));
			continue;
		}
		// allow port reuse.
		int reuse = 1;
		if (-1 == ::setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse,
				sizeof(reuse)))
		{
			LOG(DEBUG, "setsockopt failed[err %d:%s].", errno, strerror(errno));
			close(sfd);
			continue;
		}
		if (0 == ::bind(sfd, rp->ai_addr, rp->ai_addrlen))
		{
			LOG(DEBUG, "bind successfully!");
			break;
		}
		else
		{
			LOG(DEBUG, "bind err[err %d:%s].", errno, strerror(errno));
			close(sfd);
		}
	}
	LOG(DEBUG, "[addrinfo cnt:%d]", cnt);

	if(NULL == rp)
	{
		LOG(WARNING, "could not bind.");
		return -1;
	}

	freeaddrinfo(result);
	return sfd;
}

int server::_make_socket_non_blocking(int sfd)
{
	int flags = fcntl(sfd, F_GETFL, 0);
	if (-1 == flags)
	{
		LOG(WARNING, "fcntl get flag failed[err %d:%s].", errno, strerror(errno));
		return -1;
	}

	flags |= O_NONBLOCK;
	if (-1 == fcntl(sfd, F_SETFL, flags))
	{
		LOG(WARNING, "fcntl set flag O_NONBLOCK failed[err %d:%s].", errno,
				strerror(errno));
		return -1;
	}
	return 0;
}

int server::run(NEW_CONN_CALLBACK& new_conn_callback)
{
	_new_conn_callback = new_conn_callback;
	_listen_fd = _create_and_bind(_listen_port);
	if (-1 == _listen_fd)
	{
		LOG_WARNING("failed to create and bind.");
		return -1;
	}

	if (0 != epoll_scheduler::make_fd_non_blocking(_listen_fd))
	{
		LOG_WARNING("failed to make_socket_non_blocking.");
		return -1;
	}

	if (0 != ::listen(_listen_fd, SOMAXCONN))
	{
		LOG_WARNING("failed to listen on socket[err %d:%s].", errno,
						strerror(errno));
		return -1;
	}

	if(0 != _es.monitor_read_continously(_listen_fd, &_coro))
	{
		LOG_WARNING("failed to monitor_read_continously on socket[err %d:%s].", errno,
						strerror(errno));
		return -1;
	}

	// run event loop.
	_run_event_loop();
	LOG_DEBUG("event loop ends.");
	return 0;
}

int server::_handle_new_conn_async()
{
	/* We have a notification on the listening socket, which
	 means one or more incoming connections. */
	while (true)
	{
		int infd = _accept_new_conn();
		// nonnegative means an right fd.
		if(infd < 0)
		{
			break;
		}
		// call new conn callback.
		int if_ok = _handle_new_conn(infd);
		if(0 != if_ok)
		{
			LOG_WARNING("failed to accept new conn.");
			::close(infd);
			return -1;
		}
	}
	return 0;
}

int server::_handle_new_conn(int fd)
{
	// make callback to use-given callback. attention exceptions may throw in
	// given callback.
	try
	{
		_new_conn_callback(fd, this);
	}
	catch(std::exception& e)
	{
		LOG_WARNING("exception catched in _handle_new_conn[%s]", e.what());
		return -1;
	}
	catch(...)
	{
		LOG_WARNING("exception catched int _handle_new_conn.");
		return -1;
	}
	return 0;
}

int server::_accept_new_conn()
{
	struct sockaddr in_addr;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	socklen_t in_len = sizeof(in_addr);
	int infd = ::accept(_listen_fd, &in_addr, &in_len);
	if (-1 == infd)
	{
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
		{
			// no more incoming connections.
			return -1;
		}
		else
		{
			LOG_WARNING("accept failed[err %d:%s].", errno,
					strerror(errno));
			return -1;
		}
	}

	if (0 == getnameinfo(&in_addr, in_len, hbuf, sizeof(hbuf),
					sbuf, sizeof(sbuf),
					NI_NUMERICHOST | NI_NUMERICSERV))
	{
		LOG_DEBUG("accepted connection on [fd:%d][host:%s]"
				"[port:%s]", infd, hbuf, sbuf);
	}

	/* Make the incoming socket non-blocking and add it to the
	 list of fds to monitor.*/
	if (0 != epoll_scheduler::make_fd_non_blocking(infd))
	{
		LOG_WARNING("make_socket_non_blocking failed. this "
				"connection will be closed and refused.");
		::close(infd);
		return -1;
	}

	struct epoll_event event;
	event.data.fd = infd;
	// only monitor err initially.
	event.events = EPOLLERR | EPOLLHUP;
	if (-1 == epoll_ctl(_epollfd, EPOLL_CTL_ADD, infd, &event))
	{
		LOG_WARNING("epoll_ctl failed[err %d:%s]this connection "
			"will be closed and refused.", errno, strerror(errno));
		close(infd);
		return -1;
	}
	return infd;
}





