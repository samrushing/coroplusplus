// -*- Mode: C++ -*-

#include <queue>

typedef std::queue <coro *, std::deque <coro *> > coro_queue;

class condition_variable {

private:
  coro_queue _waiting;

public:

  // XXX how to interrupt something in wait()?
  void *
  wait()
  {
    coro * me = coro::current();
    if (me == 0) {
      throw "wait() from main";
    } else {
      _waiting.push (me);
      // XXX exception handler + remove
      return me->yield (0);
    }
  }

  bool
  wake_all (void * arg)
  {
    while (_waiting.size() > 0) {
      coro * co = _waiting.front(); _waiting.pop();
      // XXX handle schedule error
      co->schedule (arg);
      return true;
    }
    return false;
  }

  bool
  wake_one (void * arg)
  {
    if (_waiting.size() > 0) {
      coro * co = _waiting.front(); _waiting.pop();
      // XXX handle schedule error
      co->schedule (arg);
      return true;
    } else {
      return false;
    }
  }

};

class latch {
private:
  condition_variable _cv;
  bool _done;
public:
  latch() { _done = false; }

  void
  wait() {
    if (!_done) {
      _cv.wait();
    }
  }

  void
  wake_all() {
    _done = true;
    _cv.wake_all (0);
  }

};

template <typename T>
class fifo {
  condition_variable _cv;
  std::queue <T, std::deque <T> > _q;
public:
  void
  push (T x)
  {
    _q.push (x);
    _cv.wake_one(0);
  }
  T
  pop ()
  {
    while (_q.size() == 0) {
      _cv.wait();
    }
    T result = _q.front();
    _q.pop();
    return result;
  }
};
