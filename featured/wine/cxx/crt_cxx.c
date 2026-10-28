/* Start-up for C++ DLLs built on Wine's C runtime with clang
 * (target x86_64-windows): what a Windows C++ DLL takes from the
 * C runtime's static start-up and Wine's winecrt0 does not give,
 * since Wine has no C++ of its own.
 *
 * - the entry: C and C++ initializers, a per-DLL exit list
 * - the TLS directory
 * - thread_local constructors, run on a thread's first use
 * - thread-safe initialization of function-local statics
 */

#include <stdarg.h>
#include <limits.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

typedef void (__cdecl *_PVFV)(void);
typedef int  (__cdecl *_PIFV)(void);

typedef struct
{
    _PVFV *first;
    _PVFV *last;
    _PVFV *end;
} onexit_table;

int __cdecl _initialize_onexit_table( onexit_table *table );
int __cdecl _register_onexit_function( onexit_table *table, _PVFV fn );
int __cdecl _execute_onexit_table( onexit_table *table );

/* initializers lie between these markers: the linker sorts the
 * sections by the text after $ */
#pragma section(".CRT$XIA", read)
#pragma section(".CRT$XIZ", read)
#pragma section(".CRT$XCA", read)
#pragma section(".CRT$XCZ", read)
#pragma section(".CRT$XDA", read)
#pragma section(".CRT$XDZ", read)
#pragma section(".CRT$XLA", read)
#pragma section(".CRT$XLZ", read)

__declspec(allocate(".CRT$XIA")) static _PIFV xi_a[] = { 0 };
__declspec(allocate(".CRT$XIZ")) static _PIFV xi_z[] = { 0 };
__declspec(allocate(".CRT$XCA")) static _PVFV xc_a[] = { 0 };
__declspec(allocate(".CRT$XCZ")) static _PVFV xc_z[] = { 0 };
__declspec(allocate(".CRT$XDA")) static _PVFV xd_a[] = { 0 };
__declspec(allocate(".CRT$XDZ")) static _PVFV xd_z[] = { 0 };
__declspec(allocate(".CRT$XLA")) static PIMAGE_TLS_CALLBACK xl_a = 0;
__declspec(allocate(".CRT$XLZ")) __attribute__((used)) static PIMAGE_TLS_CALLBACK xl_z = 0;

/* ---- the TLS directory; the linker points the image at _tls_used */

ULONG _tls_index = 0;

#pragma section(".tls", read, write)
#pragma section(".tls$ZZZ", read, write)
__declspec(allocate(".tls")) char _tls_start = 0;
__declspec(allocate(".tls$ZZZ")) char _tls_end = 0;

const IMAGE_TLS_DIRECTORY64 _tls_used =
{
    (ULONGLONG)&_tls_start,
    (ULONGLONG)&_tls_end,
    (ULONGLONG)&_tls_index,
    (ULONGLONG)(&xl_a + 1),
    0,
    0
};

/* ---- the exit list: atexit runs at this DLL's unload */

static onexit_table exit_table;

int __cdecl atexit( _PVFV fn )
{
    return _register_onexit_function( &exit_table, fn );
}

/* ---- thread_local objects with constructors: the compiler checks
 * __tls_guard on a thread's first use and calls us */

__declspec(thread) char __tls_guard = 0;

void __cdecl __dyn_tls_on_demand_init( void )
{
    _PVFV *fn;

    __tls_guard = 1;
    for (fn = xd_a + 1; fn < xd_z; fn++)
        if (*fn) (*fn)();
}

/* the same as each thread starts, as the C runtime's TLS callback */
void NTAPI __dyn_tls_init( void *inst, DWORD reason, void *reserved )
{
    if (reason != DLL_THREAD_ATTACH && reason != DLL_PROCESS_ATTACH) return;
    if (!__tls_guard) __dyn_tls_on_demand_init();
}

#pragma section(".CRT$XLC", read)
__declspec(allocate(".CRT$XLC")) __attribute__((used))
static PIMAGE_TLS_CALLBACK dyn_tls_callback = __dyn_tls_init;

/* ---- type_info's vtable: the compiler makes type_info objects as
 * data and never runs a constructor that would emit it, so the C
 * runtime defines it once per module. Its one slot, the deleting
 * destructor, has nothing to free: those objects are static */

__attribute__((used))
static void * __cdecl type_info_deleting_dtor( void *self, unsigned int flags )
{
    return self;
}

__asm__( ".section .rdata,\"dr\"\n\t"
         ".p2align 3\n\t"
         ".quad 0\n\t"
         ".globl \"??_7type_info@@6B@\"\n"
         "\"??_7type_info@@6B@\":\n\t"
         ".quad type_info_deleting_dtor\n\t"
         ".text" );

/* ---- thread-safe statics, as the MSVC ABI asks: a guard above the
 * thread's epoch is not yet initialized as this thread has seen */

int _Init_global_epoch = INT_MIN;
__declspec(thread) int _Init_thread_epoch = INT_MIN;

static SRWLOCK init_lock = SRWLOCK_INIT;
static CONDITION_VARIABLE init_done = CONDITION_VARIABLE_INIT;

void __cdecl _Init_thread_header( int *once )
{
    AcquireSRWLockExclusive( &init_lock );
    for (;;)
    {
        if (*once == 0)
        {
            /* this thread runs the initializer */
            *once = -1;
            break;
        }
        if (*once != -1)
        {
            /* finished by another thread meanwhile */
            _Init_thread_epoch = _Init_global_epoch;
            break;
        }
        SleepConditionVariableSRW( &init_done, &init_lock, INFINITE, 0 );
    }
    ReleaseSRWLockExclusive( &init_lock );
}

void __cdecl _Init_thread_footer( int *once )
{
    AcquireSRWLockExclusive( &init_lock );
    _Init_global_epoch++;
    *once = _Init_global_epoch;
    _Init_thread_epoch = _Init_global_epoch;
    ReleaseSRWLockExclusive( &init_lock );
    WakeAllConditionVariable( &init_done );
}

void __cdecl _Init_thread_abort( int *once )
{
    AcquireSRWLockExclusive( &init_lock );
    *once = 0;
    ReleaseSRWLockExclusive( &init_lock );
    WakeAllConditionVariable( &init_done );
}

/* ---- the entry: initializers before DllMain, the exit list after */

BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, void *reserved );

BOOL WINAPI __cxx_dll_entry( HINSTANCE inst, DWORD reason, void *reserved )
{
    BOOL ok;

    if (reason == DLL_PROCESS_ATTACH)
    {
        _PIFV *init;
        _PVFV *ctor;

        _initialize_onexit_table( &exit_table );
        for (init = xi_a + 1; init < xi_z; init++)
            if (*init && (*init)()) return FALSE;
        for (ctor = xc_a + 1; ctor < xc_z; ctor++)
            if (*ctor) (*ctor)();
    }
    ok = DllMain( inst, reason, reserved );
    if (reason == DLL_PROCESS_DETACH)
        _execute_onexit_table( &exit_table );
    return ok;
}
