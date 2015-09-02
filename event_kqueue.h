// -*- Mode: C++ -*-

#ifndef _EVENT_KQUEUE_H
#define _EVENT_KQUEUE_H

#include <sys/time.h>
#include <sys/types.h>
#include <sys/event.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <list>
#include <iostream>

#include "coro++.h"

typedef void (*kevent_handler)(struct kevent *);

void default_signal_handler (struct kevent * kev)
{
  std::cerr << "[received " << kev->ident << " signal, exiting...]\n";
  coro::exit();
}

class kqueue_poller : public poller {

 protected:
  int _output_max;
  int _input_max;
  int _kq_fd;
  int _change_index;
  struct kevent * _change_list;

  void
  set_wait_for (int fd, coro * c, short filter)
  {
    struct kevent * ke = & _change_list[_change_index];
    ke->ident  = fd;
    ke->filter = filter;
    ke->flags  = EV_ADD | EV_ENABLE | EV_ONESHOT;
    ke->fflags = 0;
    ke->data   = 0;
    ke->udata  = (void *) c;
    // check for overflow
    _change_index++;
  }

 public:

  void
  set_signal_handler (int signum, kevent_handler fun)
  {
    // disable normal signal processing
    signal (signum, SIG_IGN);
    struct kevent * ke = & _change_list[_change_index];
    ke->ident = signum;
    ke->filter = EVFILT_SIGNAL;
    ke->flags = EV_ADD | EV_ONESHOT;
    ke->fflags = 0;
    ke->data = 0;
    ke->udata = (void *) fun;
    // XXX check for overflow
    _change_index++;
  }

  kqueue_poller (int input_max=2500, int output_max=2500)
  {
    _input_max = input_max;
    _output_max = output_max;
    _kq_fd = kqueue();
    _change_list = new struct kevent [_output_max];
    _change_index = 0;
  }

  ~kqueue_poller()
  {
    delete _change_list;
    _change_list = 0;
  }

  void set_wait_for_read  (int fd, coro * c) { set_wait_for (fd, c, EVFILT_READ); _rb++; }
  void set_wait_for_write (int fd, coro * c) { set_wait_for (fd, c, EVFILT_WRITE); _wb++; }

  void wait_for_read (int fd, coro * c)
  {
    set_wait_for_read (fd, c);
    int error = (uintptr_t)c->yield();
    if (error) {
      throw OS_Error ("wait_for_read", error);
    }
  }

  void wait_for_write (int fd, coro * c)
  {
    set_wait_for_write (fd, c);
    int error = (uintptr_t)c->yield();
    if (error) {
      throw OS_Error ("wait_for_write", error);
    }
  }

  void
  poll (int64_t timeout_usec)
  {
    if (timeout_usec < 0) {
      timeout_usec = 0;
      std::cerr << "poll() timeout < 0 ??" << std::endl;
    }
    // output
    struct kevent event_list[_output_max];
    struct timespec ts = { timeout_usec / 1000000, (timeout_usec % 1000000) * 1000};

    // std::cerr << "_change_index=" << _change_index << "\n";

    int n = kevent (_kq_fd, _change_list, _change_index, event_list, _output_max, &ts);

    _change_index = 0;

    if (n >= 0) {
      for (int i = 0; i < n; i++) {
	struct kevent * kp = &(event_list[i]);
	coro * c = (coro *) kp->udata;
	//std::cerr << "kev: " << kp->ident << ", " << kp->filter << ", " << kp->flags << ", " << kp->data << "\n";
	if ((kp->flags & EV_EOF) && (kp->fflags != 0)) {
	  std::cerr << "kevent scheduling with error:" << kp->fflags << "\n";
	  c->schedule ((void*) (uintptr_t) kp->fflags);
	} else {
	  c->schedule (0);
	}
      }
    } else {
      throw OS_Error ("kevent", n);
    }
  }
};

#endif // _EVENT_KQUEUE_H
