/* MIPS I has no sync: a signal fence only orders the compiler */
#define __c11_atomic_signal_fence(order) __asm__ volatile("" ::: "memory")
