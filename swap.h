// -*- Mode: C -*-

#ifndef _SWAP_H
#define _SWAP_H

#include <stdint.h>
#include <stdio.h>

// __swap() manages the switch from one coroutine to another.
// It's used for swaps both to and from <main>.  It preserves
// the stack, frame, and insn pointers of the previously-running
// coro, and installs the new values from coro being swapped in.
// The contents of the stack have already been put in place by
// the scheduler._restore() function.

extern "C"
{

#ifdef __i386__

typedef struct {
  void * stack_pointer;
  void * frame_pointer;
  void * insn_pointer;
  void * ebx;
  void * esi;
  void * edi;
} machine_state;

__asm__ (
".globl ___swap                                          \n"
".globl __swap                                           \n"
"	.align	4                                        \n"
"___swap:                                                \n"
"__swap:                                                 \n"
"	movl 8(%esp), %edx      # fs->%edx               \n"
"	movl %esp, 0(%edx)      # save stack_pointer     \n"
"	movl %ebp, 4(%edx)      # save frame_pointer     \n"
"	movl (%esp), %eax       # save insn_pointer      \n"
"	movl %eax, 8(%edx)                               \n"
"       movl %ebx, 12(%edx)     # save ebx,esi,edi       \n"
"       movl %esi, 16(%edx)                              \n"
"       movl %edi, 20(%edx)                              \n"
"	movl 4(%esp), %edx      # ts->%edx               \n"
"       movl 20(%edx), %edi     # restore ebx,esi,edi    \n"
"       movl 16(%edx), %esi                              \n"
"       movl 12(%edx), %ebx                              \n"
"	movl 4(%edx), %ebp      # restore frame_pointer  \n"
"	movl 0(%edx), %esp      # restore stack_pointer  \n"
"       movl 8(%edx), %eax      # restore insn_pointer   \n"
"       movl %eax, (%esp)                                \n"
"	ret                                              \n"
);

#elif defined (__x86_64__)

typedef struct {
  void * stack_pointer;
  void * frame_pointer;
  void * insn_pointer;
  void * ebx;
  void * r12;
  void * r13;
  void * r14;
  void * r15;
} machine_state;


/*
 * x86_64 calling convention: args are in rdi,rsi,rdx,rcx,r8,r9
 */

__asm__ (
".globl ___swap                                          \n"
".globl __swap                                           \n"
"___swap:                                                \n"
"__swap:                                                 \n"
"	movq %rsp, 0(%rsi)      # save stack_pointer     \n"
"	movq %rbp, 8(%rsi)      # save frame_pointer     \n"
"	movq (%rsp), %rax       # save insn_pointer      \n"
"	movq %rax, 16(%rsi)                              \n"
"	movq %rbx, 24(%rsi)     # save rbx,r12-r15       \n"
"	movq %r12, 32(%rsi)                              \n"
"	movq %r13, 40(%rsi)                              \n"
"	movq %r14, 48(%rsi)                              \n"
"	movq %r15, 56(%rsi)                              \n"
"	movq 56(%rdi), %r15                              \n"
"	movq 48(%rdi), %r14                              \n"
"	movq 40(%rdi), %r13     # restore rbx,r12-r15    \n"
"	movq 32(%rdi), %r12                              \n"
"	movq 24(%rdi), %rbx                              \n"
"	movq 8(%rdi), %rbp      # restore frame_pointer  \n"
"	movq 0(%rdi), %rsp      # restore stack_pointer  \n"
"	movq 16(%rdi), %rax     # restore insn_pointer   \n"
"	movq %rax, (%rsp)                                \n"
"	ret                                              \n"
);

#else
#error machine state not defined for this architecture
#endif

uint64_t
read_timestamp_counter (void)
{
  unsigned long a, d;
  asm volatile ("rdtsc" : "=a"(a), "=d"(d));
  return (((uint64_t) d << 32) | a);
}

int __swap (machine_state * to_state, machine_state * from_state);
void _wrap1 (coro * co);
void * yield (void * arg);

#ifdef __x86_64__
void
_wrap0 (coro * co)
{
  // x86_64 passes args in registers.  but coro.__create() puts
  // the coroutine on the stack.  fetch it from there.
#ifdef __llvm__
  // llvm does the prologue differently...
  __asm__ ("movq 16(%%rbp), %[co]" : [co] "=r" (co));
#else
  __asm__ ("movq 8(%%rbp), %[co]" : [co] "=r" (co));
  fprintf (stderr, "gcc!\n");
#endif
  _wrap1 (co);
  yield (0);
}

#else
void
_wrap0 (coro * co)
{
  _wrap1 (co);
  yield (0);
}
#endif


}

#endif // _SWAP_H
