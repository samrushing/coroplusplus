// -*- Mode: C++ -*-

#include <sys/time.h>
#include <sys/resource.h>
#include <algorithm>

class bench {
 public:
  struct rusage _ru;
  struct timeval _tv;

  void
  set() {
    gettimeofday (&_tv, 0);
    getrusage (RUSAGE_SELF, &_ru);
  }

  bench
  diff()
  {
    bench o;
    bench r;
    o.set();
    timersub (&o._tv, &_tv, &r._tv);
    timersub (&o._ru.ru_utime, &_ru.ru_utime, &r._ru.ru_utime);
    timersub (&o._ru.ru_stime, &_ru.ru_stime, &r._ru.ru_stime);
    r._ru.ru_maxrss    = std::max (_ru.ru_maxrss, o._ru.ru_maxrss);
    // hmmm.. what to do with the 'integral' values?
    r._ru.ru_ixrss     = o._ru.ru_ixrss;
    r._ru.ru_idrss     = o._ru.ru_idrss;
    r._ru.ru_isrss     = o._ru.ru_isrss;
    r._ru.ru_minflt    = o._ru.ru_minflt   - _ru.ru_minflt;
    r._ru.ru_majflt    = o._ru.ru_majflt   - _ru.ru_majflt;
    r._ru.ru_nswap     = o._ru.ru_nswap    - _ru.ru_nswap;
    r._ru.ru_inblock   = o._ru.ru_inblock  - _ru.ru_inblock;
    r._ru.ru_oublock   = o._ru.ru_oublock  - _ru.ru_oublock;
    r._ru.ru_msgsnd    = o._ru.ru_msgsnd   - _ru.ru_msgsnd;
    r._ru.ru_msgrcv    = o._ru.ru_msgrcv   - _ru.ru_msgrcv;
    r._ru.ru_nsignals  = o._ru.ru_nsignals - _ru.ru_nsignals;
    r._ru.ru_nvcsw     = o._ru.ru_nvcsw    - _ru.ru_nvcsw;
    r._ru.ru_nivcsw    = o._ru.ru_nivcsw   - _ru.ru_nivcsw;
    return r;
  }

  void
  dump (std::string &r)
  {
    // There's _got_ to be a better way to say this.
    char buffer[512];
    int n = snprintf (
      buffer,
      sizeof (buffer),
      "%ld %06ld|%ld %06ld|%ld %06ld|"
      "maxrss:%ld "
      "ixrss:%ld "
      "idrss:%ld "
      "isrss:%ld "
      "minflt:%ld "
      "majflt:%ld "
      "nswap:%ld "
      "inblock:%ld "
      "oublock:%ld "
      "msgsnd:%ld "
      "msgrcv:%ld "
      "nsignals:%ld "
      "nvcsw:%ld "
      "nivcsw:%ld"
      "\r\n",
      (long)_tv.tv_sec,
      (long)_tv.tv_usec,
      (long)_ru.ru_utime.tv_sec,
      (long)_ru.ru_utime.tv_usec,
      (long)_ru.ru_stime.tv_sec,
      (long)_ru.ru_stime.tv_usec,
      _ru.ru_maxrss,
      _ru.ru_ixrss,
      _ru.ru_idrss,
      _ru.ru_isrss,
      _ru.ru_minflt,
      _ru.ru_majflt,
      _ru.ru_nswap,
      _ru.ru_inblock,
      _ru.ru_oublock,
      _ru.ru_msgsnd,
      _ru.ru_msgrcv,
      _ru.ru_nsignals,
      _ru.ru_nvcsw,
      _ru.ru_nivcsw
      );
    r += std::string (buffer, n);
  }

};
