// -*- Mode: C++ -*-

#include <map>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <poll.h>

#include "coro++.h"

typedef std::map <int, coro *, std::less <int> > fd_map;

class poll_poller : public poller {

 private:
  fd_map _read_set;
  fd_map _write_set;
  struct pollfd * _poll_set;

 public:

  poll_poller (int size)
  {
    _poll_set = new struct pollfd[size];
  }

  void
  set_wait_for_read (int fd, coro * c)
  {
    _read_set[fd] = c;
  }

  void
  set_wait_for_write (int fd, coro * c)
  {
    _write_set[fd] = c;
  }

  void
  poll (int64_t timeout_usec)
  {
    int count = _read_set.size() + _write_set.size();

    fd_map::iterator i;
    int j = count;

    for (i = _read_set.begin(); i != _read_set.end(); i++) {
      int fd = (*i).first;;
      j--;
      _poll_set[j].fd = fd;
      _poll_set[j].events |= POLLIN;
    }

    for (i = _write_set.begin(); i != _write_set.end(); i++) {
      int fd = (*i).first;;
      j--;
      _poll_set[j].fd = fd;
      _poll_set[j].events |= POLLOUT;
    }

    //std::cerr << "poll(" << count << ")";

    //std::cerr << "poll timeout=" << timeout_usec / 1000 << std::endl;

    int poll_result = ::poll (_poll_set, count, timeout_usec / 1000);

    //std::cerr << "=" << poll_result << "\n";

    if (poll_result < 0) {
      std::cerr << "poll() failed: " << errno << "\n";
    } else {
      for (j = 0; ((j < count) && poll_result--); j++) {
	//std::cerr << _poll_set[j].fd << ":" << _poll_set[j].revents << std::endl;
	if (_poll_set[j].revents & POLLIN) {
	  i = _read_set.find (_poll_set[j].fd);
	  if (i != _read_set.end()) {
	    ((*i).second)->schedule();
	    _read_set.erase (i);
	  } else {
	    std::cerr << "fd " << _poll_set[j].fd << "not in read set!" << std::endl;
	  }
	} else if (_poll_set[j].revents & POLLOUT) {
	  i = _write_set.find (_poll_set[j].fd);
	  if (i != _write_set.end()) {
	    ((*i).second)->schedule();
	    _write_set.erase (i);
	  } else {
	    std::cerr << "fd " << _poll_set[j].fd << "not in read set!" << std::endl;
	  }
	}
      }
    }
  }

};
