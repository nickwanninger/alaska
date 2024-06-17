# The Alaska Runtime

This directory contains the source code for the Alaska runtime system.
Logically, it is broken down into two components: the `core` and the `rt`.


## The Core Runtime

The core of the runtime is built up around a single class, `alaska::Runtime`, and is designed to be end-to-end testable.
It breaks down all of the concepts in the runtime into obvious classes which do just what they need to.
For example, the region of memory assigned to the linear handle table is simply a class, `alaska::HandleTable` which subdivides the table to allow allocations.
Similarly, the Heap datastructure encapsulates the virtual memory assigned to the heap, as well as subdividing it into `HeapPages` which are abstract enough to allow any interesting allocation policy you might have.
Maybe it's a little to OOP-y, but thats okay!
It's a very easily testable design, which is important because the original verison of this runtime as it was submitted to ASPLOS'24 was *not* stable enough to hold up future research on handle-based systems.


That said, if you link against the Alaska runtime core (`alaska_core.a`), you can construct a runtime as follows:
```c++
int main() {
    // Simply allocate the runtime
    alaska::Runtime rt;
}
```

It's important to note that, because of some limitations to the software approach of alaska currently, there can only be *one* runtime allocated at a time.
This is because things like the handle table must be located in a *very specific location in memory* in order to allow efficient translation in software.
We expect as we investigate hardware implementations of alaska, these restrictions will be lifted -- though it doesn't make much sense to have multiple runtimes anyways.

Once a runtime is allocated, you can simply use `alaska::Runtime::get()` from anywhere to access that runtime instance.
The `alaska::Runtime` class itself provides very little functionality by design, as the majority of access to the Alaska heap must be done through a *thread cache*.
To allocate a thread cache, simply use the runtime and allocate a new one: `auto tc = rt.new_threadcache()`.
Note that the runtime does not actually care which thread this thread cache is used from, and assigning and managing these is up to the specific runtime system policies (see below).


Given this thread cache (`tc`), allocation requests can be easily made using `tc->halloc(size);` or `tc->hfree(handle)`.
The only restriction on using a thread cache is that it is **NOT THREAD SAFE**.
If you wanted, you could allocate a single global thread cache, and hide it behind a mutex lock.


The thread cache is very closely modeled after classic allocators like Hoard.
This means that a thread is only ever allowed to touch *its* heap, and the global heap.
Allocation is thus entirely lock-free, ideally.
When an allocation request is made, it is first routed to the correct size class, which in turn may allocate a `HeapPage` (in particular, `SizedPage`), and the object is then bump allocated from that page.
If that object is freed, it is added to a thread-private free list.
If another object is allocated it is then trivially popped from that free list.
If an object in one thread's cache is freed by another, the `SizedPage` allocator will add that object to a "global free list" for that page.
This is exactly how mimalloc works, for example.

In the event of the private free list being empty, and the bump allocator having no more space, the heap page will simply swap the global and private free lists and try to allocate again.
If an allocation cannot be made, the full page is left behind and a new page is requested from the global heap.

> **Note**: I have not yet figured out how to deal with reclamation of full or partially full pages. I think there is something interesting to do there w.r.t. compaction and defragmentation, though


## The Runtime System (rt)

This system provides binding and initializers to *applications* written using Alaska handles.
Importantly, it provides things like the `halloc` and `hfree` symbols, but it also manages assigning thread caches to threads, routing memory allocation requests, and the delivery of barrier signals which signal actions like memory compaction which rely on pin sets.

> **Note**: pin sets are not required when targeting systems which translate handles in *hardware*
