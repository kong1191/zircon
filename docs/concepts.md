# Zircon Kernel Concepts

## Introduction

The kernel manages a number of different types of Objects. Those which are
accessible directly via system calls are C++ classes which implement the
Dispatcher interface. These are implemented in
[kernel/object](../kernel/object). Many are self-contained higher-level Objects.
Some wrap lower-level lk primitives.

## [System Calls](syscalls.md)

Userspace code interacts with kernel objects via system calls, and almost exclusively
via Handles.  In userspace, a Handle is represented as 32bit integer
(type zx_handle_t).  When syscalls are executed, the kernel checks that Handle
parameters refer to an actual handle that exists within the calling process's handle
table.  The kernel further checks that the Handle is of the correct type (passing
a Thread Handle to a syscall requiring an event handle will result in an error),
and that the Handle has the required Rights for the requested operation.

System calls fall into three broad categories, from an access standpoint:

1. Calls which have no limitations, of which there are only a very few, for
example [*zx_clock_get()*](syscalls/clock_get.md)
and [*zx_nanosleep()*](syscalls/nanosleep.md) may be called by any thread.
2. Calls which take a Handle as the first parameter, denoting the Object they act upon,
which are the vast majority, for example [*zx_channel_write()*](syscalls/channel_write.md)
and [*zx_port_queue()*](syscalls/port_queue.md).
3. Calls which create new Objects but do not take a Handle, such as
[*zx_event_create()*](syscalls/event_create.md) and
[*zx_channel_create()*](syscalls/channel_create.md).  Access to these (and limitations
upon them) is controlled by the Job in which the calling Process is contained.

System calls are provided by libzircon.so, which is a "virtual" shared
library that the Zircon kernel provides to userspace, better known as the
[*virtual Dynamic Shared Object* or vDSO](vdso.md).
They are C ELF ABI functions of the form *zx_noun_verb()* or
*zx_noun_verb_direct-object()*.

The system calls are defined by [syscalls.abigen](../system/public/zircon/syscalls.abigen)
and processed by the [abigen](../system/host/abigen/) tool into include files and glue
code in libzircon and the kernel's libsyscalls.


## [Handles](handles.md) and [Rights](rights.md)

Objects may have multiple Handles (in one or more Processes) that refer to them.

For almost all Objects, when the last open Handle that refers to an Object is closed,
the Object is either destroyed, or put into a final state that may not be undone.

Handles may be moved from one Process to another by writing them into a Channel
(using [*zx_channel_write()*](syscalls/channel_write.md)), or by using
[*zx_process_start()*](syscalls/process_start.md) to pass a Handle as the argument
of the first thread in a new Process.

The actions which may be taken on a Handle or the Object it refers to are governed
by the Rights associated with that Handle.  Two Handles that refer to the same Object
may have different Rights.

The [*zx_handle_duplicate()*](syscalls/handle_duplicate.md) and
[*zx_handle_replace()*](syscalls/handle_replace.md) system calls may be used to
obtain additional Handles referring to the same Object as the Handle passed in,
optionally with reduced Rights.  The [*zx_handle_close()*](syscalls/handle_close.md)
system call closes a Handle, releasing the Object it refers to, if that Handle is
the last one for that Object.


## Kernel Object IDs

Every object in the kernel has a "kernel object id" or "koid" for short.
It is a 64 bit unsigned integer that can be used to identify the object
and is unique for the lifetime of the running system.
This means in particular that koids are never reused.

There are two special koid values:

*ZX_KOID_INVALID* Has the value zero and is used as a "null" sentinel.

*ZX_KOID_KERNEL* There is only one kernel, and it has its own koid.


## Running Code: Jobs, Processes, and Threads.

Threads represent threads of execution (CPU registers, stack, etc) within an
address space which is owned by the Process in which they exist.  Processes are
owned by Jobs, which define various resource limitations.  Jobs are owned by
parent Jobs, all the way up to the Root Job which was created by the kernel at
boot and passed to [`userboot`, the first userspace Process to begin execution](userboot.md).

Without a Job Handle, it is not possible for a Thread within a Process to create another
Process or another Job.

[Program loading](program_loading.md) is provided by userspace facilities and
protocols above the kernel layer.

See: [process_create](syscalls/process_create.md),
[process_start](syscalls/process_start.md),
[thread_create](syscalls/thread_create.md),
and [thread_start](syscalls/thread_start.md).


## Message Passing: Sockets and Channels

Both Sockets and Channels are IPC Objects which are bi-directional and two-ended.
Creating a Socket or a Channel will return two Handles, one referring to each endpoint
of the Object.

Sockets are stream-oriented and data may be written into or read out of them in units
of one or more bytes.  Short writes (if the Socket's buffers are full) and short reads
(if more data is requested than in the buffers) are possible.

Channels are datagram-oriented and have a maximum message size of 64K (subject to change,
likely to be smaller) and may also have up to 1024 Handles attached to a message (also
subject to change, also likely to be smaller).  They do not support short reads or writes --
either a message fits or it does not.

When Handles are written into a Channel, they are removed from the sending Process.
When a message with Handles is read from a Channel, the Handles are added to the receiving
Process.  Between these two events, the Handles continue to exist (ensuring the Objects
they refer to continue to exist), unless the end of the Channel which they have been written
towards is closed -- at which point messages in flight to that endpoint are discarded and
any Handles they contained are closed.

See: [channel_create](syscalls/channel_create.md),
[channel_read](syscalls/channel_read.md),
[channel_write](syscalls/channel_write.md),
[channel_call](syscalls/channel_call.md),
[socket_create](syscalls/socket_create.md),
[socket_read](syscalls/socket_read.md),
and [socket_write](syscalls/socket_write.md).

## Objects and Signals

Objects may have up to 32 signals (represented by the zx_signals_t type and the ZX_*_SIGNAL_*
defines) which represent a piece of information about their current state.  Channels and Sockets,
for example, may be READABLE or WRITABLE.  Processes or Threads may be TERMINATED.  And so on.

Threads may wait for signals to become active on one or more Objects.

See [signals](signals.md) for more information.

## Waiting: Wait One, Wait Many, and Ports

A Thread may use [*zx_object_wait_one()*](syscalls/object_wait_one.md)
to wait for a signal to be active on a single handle or
[*zx_object_wait_many()*](syscalls/object_wait_many.md) to wait for
signals on multiple handles.  Both calls allow for a timeout after
which they'll return even if no signals are pending.

If a Thread is going to wait on a large set of handles, it is more efficient to use
a Port, which is an Object that other Objects may be bound to such that when signals
are asserted on them, the Port receives a packet containing information about the
pending Signals.

See: [port_create](syscalls/port_create.md),
[port_queue](syscalls/port_queue.md),
[port_wait](syscalls/port_wait.md),
[port_cancel](syscalls/port_cancel.md).


## Events, Event Pairs.

An Event is the simplest Object, having no other state than its collection of active Signals.

An Event Pair is one of a pair of Events that may signal each other.  A useful property of
Event Pairs is that when one side of a pair goes away (all Handles to it have been
closed), the PEER_CLOSED signal is asserted on the other side.

See: [event_create](syscalls/event_create.md),
and [eventpair_create](syscalls/eventpair_create.md).


## Shared Memory: Virtual Memory Objects (VMOs)

Virtual Memory Objects represent a set of physical pages of memory, or the *potential*
for pages (which will be created/filled lazily, on-demand).

They may be mapped into the address space of a Process with
[*zx_vmar_map()*](syscalls/vmar_map.md) and unmapped with
[*zx_vmar_unmap()*](syscalls/vmar_unmap.md).  Permissions of
mapped pages may be adjusted with [*zx_vmar_protect()*](syscalls/vmar_protect.md).

VMOs may also be read from and written to directly with
[*zx_vmo_read()*](syscalls/vmo_read.md) and [*zx_vmo_write()*](syscalls/vmo_write.md).
Thus the cost of mapping them into an address space may be avoided for one-shot operations
like "create a VMO, write a dataset into it, and hand it to another Process to use."

## Address Space Management

Virtual Memory Address Regions (VMARs) provide an abstraction for managing a
process's address space.  At process creation time, a handle to the root VMAR
is given to the process creator.  That handle refers to a VMAR that spans the
entire address space.  This space can be carved up via the
[*zx_vmar_map()*](syscalls/vmar_map.md) and
[*zx_vmar_allocate()*](syscalls/vmar_allocate.md) interfaces.
[*zx_vmar_allocate()*](syscalls/vmar_allocate.md) can be used to generate new
VMARs (called subregions or children) which can be used to group together
parts of the address space.

See: [vmar_map](syscalls/vmar_map.md),
[vmar_allocate](syscalls/vmar_allocate.md),
[vmar_protect](syscalls/vmar_protect.md),
[vmar_unmap](syscalls/vmar_unmap.md),
[vmar_destroy](syscalls/vmar_destroy.md),

## Futexes

Futexes are kernel primitives used with userspace atomic operations to implement
efficient synchronization primitives -- for example, Mutexes which only need to make
a syscall in the contended case.  Usually they are only of interest to implementers of
standard libraries.  Zircon's libc and libc++ provide C11, C++, and pthread APIs for
mutexes, condition variables, etc, implemented in terms of Futexes.

See: [futex_wait](syscalls/futex_wait.md),
[futex_wake](syscalls/futex_wake.md),
[futex_requeue](syscalls/futex_requeue.md).

## Zircon Device Index

The Zircon Device Index (MDI) is a read-only binary data structure passed from the bootloader
that contains configuration information for the kernel and various drivers in zircon.
See [mdi](mdi.md).