/*
 * coro.h
 *
 *  Created on: 2012-11-17
 *      Author: yelu
 */

#ifndef CORO_H_
#define CORO_H_

#include <unistd.h>
#include <stdint.h>
#include <stdexcept>
#include <context/all.hpp>
#include <coroutine/all.hpp>
#include "log.h"
using boost::context::fcontext_t;

#define DEFAULT_CORO_STACK_SIZE 102400


/**
 * @brief a coroutine implementation.
 */
class coro
{
public:
	coro();
	virtual ~coro();

	/*
	 * @brief initialize the ucontext_t structure (data member) to the current
	 * user context of the calling thread. then create a new stack for future use.
	 */
	void declare_as_child(coro& switch_to_when_return,
			    uint32_t stack_size = DEFAULT_CORO_STACK_SIZE);

	void switch_to(coro& to_coro);
	void yield();
	//virtual void callback() = 0;
	virtual void callback()
	{
		LOG_DEBUG("callback.");
	}
private:

	boost::coroutines::detail::stack_allocator _alloc;
	uint32_t _stack_size;
	void* _stack;
	coro* _caller;

	fcontext_t* _context;
};

void coro_callback(intptr_t);

#endif /* CORO_H_ */
