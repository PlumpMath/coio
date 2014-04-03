/*
 * coio.h
 *
 *  Created on: Apr 2, 2014
 *      Author: luye
 */

#ifndef COIO_H_
#define COIO_H_

#include "coio.h"

#define DEFAULT_FD_BUF_SIZE 1024

class fd_t
{
public:
	fd_t()
	{
		_fd = -1;
		_read_buf = new(std::nothrow) char[DEFAULT_FD_BUF_SIZE];
		_write_buf = new(std::nothrow) char[DEFAULT_FD_BUF_SIZE];
		assert(NULL != _read_buf && NULL != _write_buf);
		_unread_bytes = 0;
		_unwrite_bytes = 0;
	}
	~fd_t()
	{
		delete []_read_buf;
		delete []_write_buf;
		_read_buf = NULL;
		_write_buf = NULL;
	}
	int reset()
	{
		_unread_bytes = 0;
		_unwrite_bytes = 0;
		return 0;
	}
	int reset_fd(int fd)
	{
		_fd = fd;
		assert(fd >= 0);
		reset();
	}
	int get_fd(){return _fd;}

	void close()
	{
		::close(_fd);
		_fd = -1;
	}
private:
	int _fd;
	char* _read_buf;
	char* _write_buf;
	int _unread_bytes;
	int _unwrite_bytes;
};

/**
 * @brief a corontine implenmentation for network io.
 */
class coio:public coro
{
public:
	coio(epoll_scheduler* es);
	virtual ~coio();

	int run();

	int write(fd_t& fd, char* buf, uint64_t len);
	int read(fd_t& fd, char* buf, uint64_t len);

private:

	epoll_scheduler* _es;
};


#endif /* COIO_H_ */
