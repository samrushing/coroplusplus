
coro++
======

This is a C++ user-threading library that uses the same stack-copying
technique as [shrapnel](https://github.com/ironport/shrapnel).
Although it has not been used in production yet, I have often used it
for benchmarking and testing of shrapnel itself (and other systems).

It provides three different 'event-driven' backends - ``kqueue(2)``,
``select(2)``, and ``poll(2)``.  It should be trivial to add support
for /dev/epoll for linux.

At one point I wrote most of a spdy3 proxy using this library, though
the code is unfinished.  If anyone's interested in it let me know.

What's the Idea?
----------------

The idea is to let you write code that *appears* to be blocking, but
actually uses an event loop in the background to catch socket calls
that would block.  When a thread blocks, the current stack is copied
off onto the heap until the necessary event is triggered
(e.g. ``wait_for_read()``, ``wait_for_write()``).  When the event has
triggered, the stack is copied back into place and the code proceeds
as if nothing happened.

Shrapnel uses this technique to implement highly scalable and
performant versions of several protocols, including DNS, SMTP, LDAP,
HTTP, SPDY, websockets, postgres, mysql, AMQP, etc..

How does this differ from other threading/coroutine libraries?
--------------------------------------------------------------

The main difference is that this system gets by with just *two* stacks:
one for the main thread (the scheduler), and the other for all the other
threads.  Other systems often use a separate stack for every thread, wasting
large amounts of memory.  (Shrapnel goes a step further and provides a way
to compress the stacks of idle threads).

How?
----

A context switch is performed using a small amount of assembly, which
was originally based on the user-space setcontext() call from
FreeBSD. (FreeBSD nowadays implements this as a system call).

Only two architectures are currently supported: i386 and x86_64.
This code has been tested on linux, freebsd, and os x.

See ``swap.h`` for details.


TLS Support
-----------
Interfaces for both (OpenSSL)[https://openssl.org] and (S2N)[https://github.com/awslabs/s2n] are included.

Demo
----

The library consists only of ``.h`` files.  To build the demos:

```shell
darth:demo rushing$ make
c++ -O2 -g -Wall -I.. -c echo_server.cc
c++ -O2 -g -Wall -o echo_server echo_server.o -lstdc++ -lm
c++ -O2 -g -Wall -I.. -c smtp_server.cc
c++ -O2 -g -Wall -o smtp_server smtp_server.o -lstdc++ -lm
```

Run the echo_server:

```shell
darth:demo rushing$ ./echo_server
using kqueue()
coro::stack_base = 0x107a19000
echo_server starting on port 9001
thing thread 99 #0

exiting event loop
exiting main coroutine
read_block: 10087  write_block: 0
```

In another terminal:

```shell
darth:demo rushing$ ./echo_client 127.0.0.1 9001 100 100 100

exiting event loop
server bench: 1 772274|0 072406|0 088307|maxrss:4591616 ixrss:0 idrss:0 isrss:0 minflt:979 majflt:0 nswap:0 inblock:0 oublock:0 msgsnd:10000 msgrcv:10100 nsignals:0 nvcsw:0 nivcsw:219
client bench: 0 164574|0 015882|0 112953|maxrss:1232896 ixrss:0 idrss:0 isrss:0 minflt:155 majflt:0 nswap:0 inblock:0 oublock:0 msgsnd:10101 msgrcv:10001 nsignals:0 nvcsw:0 nivcsw:5083
read_block: 10101  write_block: 100
```

This does 100 transactions of 100 bytes each on 100 connections.
