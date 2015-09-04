// -*- Mode: C++ -*-

#ifndef _COROPP_H
#define _COROPP_H

#include <list>
#include <queue>
#include <iostream>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdexcept>
#include <memory>

class coro;

class poller {

public:
  long _rb, _wb;
  poller ()  : _rb (0), _wb (0) {};
  virtual ~poller () {};
  virtual void set_wait_for_read (int fd, coro * c) = 0;
  virtual void set_wait_for_write (int fd, coro * c) = 0;
  virtual void poll (int64_t timeout) {};
  virtual void wait_for_read (int fd, coro * c);
  virtual void wait_for_write (int fd, coro * c);
};


typedef std::pair <coro *, void *> pending_entry;
typedef std::list < pending_entry > pending_list;

// ---------------------------------------------------------------------------
//				events
// ---------------------------------------------------------------------------

class event {
public:
  struct timeval _t;
  void * _args;
  coro * _coro;

  event (struct timeval t, coro * c, void * args) {
    _t = t;
    _coro = c;
    _args = args;
  }

};

struct compare_events {
  bool operator () (event * a, event * b) const
    { return (timercmp (&(a->_t), &(b->_t), >)); }
};

typedef std::priority_queue <event *, std::deque <event *>, compare_events> event_list;

/*
 *
 * cdef struct machine_state:
 *     00 void * stack_pointer
 *     04 void * frame_pointer
 *     08 void * insn_pointer
 *     12 void * ebx
 *     16 void * esi
 *     20 void * edi
 *
 *
 */

#include "swap.h"

class Coro_Error : public std::runtime_error {
public:
  Coro_Error (const char * what) : std::runtime_error (what) { };
};

class Dead_Coro : public Coro_Error {
public:
  coro * _zombie;
  Dead_Coro (coro * zombie) : Coro_Error ("attempt to schedule dead coro") {
    _zombie = zombie;
  };
};

class Yield_From_Main : public Coro_Error {
public:
  Yield_From_Main() : Coro_Error ("yield from main") {};
};

class OS_Error : public Coro_Error {
public:
  const char * _fun;
  int _errno;
  OS_Error (const char * fun, int n) : Coro_Error ("OS error") {
    _fun = fun;
    _errno = n;
  };
};

// the type of a thread function
typedef void * (*spawn_fun)(void *);
// this class only exists to allow us to use a pointer-to-member-function.
//  if you want to use a member function as a thread (without a C wrapper),
//  you have to derive from this class.
class spawner {};
// the type of a thread member function
typedef void (spawner::*spawn_method)(void *);

class coro {

  // private:
public:
  static bool _exit;
  static poller * _poller;
  static event_list * _events;
  static void ** _stack_base;
  static void ** _stack_top;
  static size_t _stack_size;
  static int _id_counter;

  machine_state _state;
  void ** _stack_copy;
  size_t _stack_copy_size;

 public:

  static coro * _current;
  static pending_list * _pending;
  static pending_list * _staging;
  static coro * _main;
  void * _value;
  spawn_fun _fun;
  spawn_method _meth;
  void * _arg0, * _arg1;
  bool started, dead, scheduled;
  int _id;

  static void set_poller (poller * p) {_poller = p;}
  static poller * get_poller () { return _poller; }
  static void exit (void) { _exit = true; }

  void schedule (void * arg = 0)
  {
    if (!dead) {
      _pending->push_back ( pending_entry (this, arg) );
    } else {
      throw Dead_Coro (this);
    }
  }

  static void run_pending ()
  {
    pending_list::iterator i;
    schedule_events();
    while (_pending->size() > 0) {
      std::swap (_staging, _pending);
      for (i = _staging->begin(); i != _staging->end(); ++i) {
	coro * co = (*i).first;
	co->resume ((*i).second);
	if (!co->dead) {
	  co->preserve();
	}
      }
      _staging->clear();
      schedule_events();
    }
  }

  static coro * current () { return _current; }

  static
  void
  schedule_events()
  {
    struct timeval tv;
    ::gettimeofday (&tv, 0);
    while (_events->size() > 0) {
      event * e = _events->top();
      if (timercmp (&(e->_t), &tv, <)) {
	_events->pop();
	e->_coro->schedule (e->_args);
      } else {
	break;
      }
    }
  }

  static
  int64_t
  time_to_next_event ()
  {
    if (_events->size() > 0) {
      event * e = _events->top();
      struct timeval t0, t1;
      ::gettimeofday (&t0, 0);
      timersub (&(e->_t), &t0, &t1);
      if ((t1.tv_sec >= 0) && (t1.tv_usec > 0)) {
	return (t1.tv_sec * 1000000) + t1.tv_usec;
      } else {
	return 0;
      }
    } else {
      return 30000000; // 30s
    }
  }

  // XXX cheap check for a 'spinning' condition -
  //     every <xxx> times through the loop, see if <1s. has gone by.

  static
  void
  event_loop()
  {
    while (!_exit) {
      //std::cerr << '.';
      run_pending();
      if (!_exit) {
	int64_t until = time_to_next_event();
	coro::_poller->poll (until);
      } else {
	break;
      }
    }
    std::cerr << "\nexiting event loop\n";
  }


  // --------------------------------------------------
  static
  event *
  insert_event (struct timeval t, coro * c, void * args = 0)
  {
    event * e = new event (t, c, args);
    _events->push (e);
    return e;
  }

#if 0
  // this might not be possible with the standard priority_queue
  static
  void
  remove_event (event * e)
  {
    _events->erase (e);
  }
#endif

  // --------------------------------------------------

  void
  create()
  {
    _stack_top[-5] = 0;
    _stack_top[-4] = this;
    _state.stack_pointer = &_stack_top[-6];
    _state.frame_pointer = &_stack_top[-5];
    _state.insn_pointer  = (void *) _wrap0;
    started = true;
    //std::cerr << "stack_pointer = " << _state.stack_pointer << "\n";
  }

  coro (spawn_fun fun, void * arg0 = 0)
  {
    _fun = fun;
    _meth = 0;
    _arg0 = arg0;
    _stack_copy = 0;
    _stack_copy_size = 0;
    _id = _id_counter++;
    started = false;
    dead = false;
  }

  coro (spawn_method meth, void * arg0 = 0)
  {
    _fun = 0;
    _meth = meth;
    _arg0 = arg0;
    _stack_copy = 0;
    _stack_copy_size = 0;
    _id = _id_counter++;
    started = false;
    dead = false;
  }

  ~coro() {
    if (_stack_copy) {
      free (_stack_copy);
    }
  }

  void
  preserve()
  {
    size_t size;
    // 1) identify the slice
    size = (char *) _stack_top - (char *)_state.stack_pointer;
    //fprintf (stderr, "preserve: %p - %p = %d\n", _stack_top, _state.stack_pointer, size);
    // 2) get some storage
    _stack_copy = (void **) malloc (size);
    if (!_stack_copy) {
      throw "unable to malloc stack copy";
    }
    _stack_copy_size = size;
    // 3) make the copy
    memcpy (_stack_copy, _state.stack_pointer, size);
    // std::cerr << "[" << size << "]";
    // fprintf (stderr, "[%d] preserved %p:%d\n", _id, _state.stack_pointer, size);
  }

  void
  restore()
  {
    memcpy (_state.stack_pointer, _stack_copy, _stack_copy_size);
    //fprintf (stderr, "[%d] restored %p:%zu\n", _id, _state.stack_pointer, _stack_copy_size);
    free (_stack_copy);
    _stack_copy = 0;
  }

  void *
  resume (void * arg = 0)
  {
    if (!dead) {
      if (!started) {
        create();
      }
      _current = this;
      if (_stack_copy) {
        restore();
      }
      //std::cerr << "[" << _id << "]" << " resume\n";
      _value = arg;
      __swap (&_state, &_main->_state);
      return _main->_value;
    } else {
      throw Dead_Coro (this);
    }
  }

  void * yield (void * arg = 0)
  {
    //std::cerr << "[" << _id << "]" << " yield\n";
    _value = arg;
    __swap (&_main->_state, &_current->_state);
    return _current->_value;
  }

  void
  sleep (int msec)
  {
    coro * me = _current;
    struct timeval tv0, tv1;
    gettimeofday (&tv0, 0);
    tv1.tv_sec = msec / 1000;
    tv1.tv_usec = (msec % 1000) * 1000;
    timeradd (&tv0, &tv1, &tv1);
    insert_event (tv1, me);
    yield();
  }

};

// sadly, these defns must come down here after the defn of coro,
//   because C++ mandates a stupid compiler.
void poller::wait_for_read (int fd, coro * c)
{
  set_wait_for_read (fd, c);
  c->yield();
};

void poller::wait_for_write (int fd, coro * c)
{
  set_wait_for_write (fd, c);
  c->yield();
};

// we allow a coroutine thread function to be either a global C
//   function, or a member function.  The deep magic required to do
//   this exposes part of the foul underbelly of C++, "pointers to
//   member functions":
//
// http://www.parashift.com/c++-faq/pointers-to-members.html
// http://yosefk.com/c++fqa/function.html
//
// C++11's closures might make this a little cleaner.  Maybe.

extern "C" {
void
_wrap1 (coro * co)
{
  try {
    if (co->_meth != 0) {
      (((spawner*)(co->_arg0))->*(co->_meth))(0);
    } else {
      co->_fun (co->_arg0);
    }
  } catch (...) {
    // XXX this is probably bad C++ policy...
    std::cerr << "unhandled exception in coro #" << co->_id << "\n";
  }
  co->dead = true;
}

void *
yield (void * arg)
{
  return coro::_current->yield (arg);
}

}

// there can be only one...
static coro _main_coroutine ((spawn_fun)0);
pending_list * coro::_pending = new pending_list;
pending_list * coro::_staging = new pending_list;
coro * coro::_current = 0;
coro * coro::_main = &_main_coroutine;
poller * coro::_poller = 0;
bool coro::_exit = false;
size_t coro::_stack_size = 4 * 1024 * 1024;
void ** coro::_stack_base = (void **) malloc (coro::_stack_size);
void ** coro::_stack_top  = coro::_stack_base + (coro::_stack_size / sizeof (void *));
int coro::_id_counter = 0;
event_list * coro::_events = new event_list;

#endif // _COROPP_H
