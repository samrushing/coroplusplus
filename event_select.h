// -*- Mode: C++ -*-

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include <map>
#include <iostream>

#include "coro++.h"

typedef std::map <int, coro *, std::less <int> > fd_map;

// concern ourselves with FD_SETSIZE

class select_poller : public poller {

 private:
  fd_map _read_set;
  fd_map _write_set;

 public:

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
    fd_set r, w;
    FD_ZERO (&r);
    FD_ZERO (&w);

    struct timeval tv = {timeout_usec / 1000000, timeout_usec % 1000000};

    fd_map::iterator i;
    int max_fd = 0;

    for (i = _read_set.begin(); i != _read_set.end(); i++) {
      int fd = (*i).first;;
      FD_SET (fd, &r);
      max_fd = std::max (fd, max_fd);
    }

    for (i = _write_set.begin(); i != _write_set.end(); i++) {
      int fd = (*i).first;;
      FD_SET (fd, &w);
      max_fd = std::max (fd, max_fd);
    }

    // std::cerr << "select(" << max_fd << ")\n";

    int n = select (max_fd + 1, &r, &w, 0, &tv);

    // std::cerr << "=" << n << "\n";

    if (n < 0) {
      std::cerr << "select() failed: " << errno << "\n";
    } else {
      while (n) {
	for (i = _read_set.begin(); i != _read_set.end(); i++) {
	  if (FD_ISSET ((*i).first, &r)) {
            ((*i).second)->schedule();
	    n--;
	    _read_set.erase (i);
	  }
	}

	for (i = _write_set.begin(); i != _write_set.end(); i++) {
	  if (FD_ISSET ((*i).first, &w)) {
            ((*i).second)->schedule();
	    n--;
	    _write_set.erase (i);
	  }
	}
      }
    }
  }

};
