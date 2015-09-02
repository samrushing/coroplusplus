// -*- Mode: C++ -*-

#ifndef _EVENT_POLLER_H
#define _EVENT_POLLER_H

#include "coro++.h"

void * yield (void * arg);

class poller {

public:

  long _rb, _wb;

  virtual void set_wait_for_read (int fd, coro * c) = 0;
  virtual void set_wait_for_write (int fd, coro * c) = 0;
  virtual void poll (int64_t timeout) {_rb = 0; _wb = 0;};
  virtual void wait_for_read (int fd, coro * c);
  virtual void wait_for_write (int fd, coro * c);
  virtual ~poller () {};

};

void poller::wait_for_read (int fd, coro * c)
{
  set_wait_for_read (fd, c);
  yield (0);
};

void poller::wait_for_write (int fd, coro * c)
{
  set_wait_for_write (fd, c);
  yield (0);
};

#endif // _EVENT_POLLER_H
