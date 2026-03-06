# Seraph Data Structure Design Choices

This document contains design choices for each data structures. It mainly discusses the memory models and locking mechanisms with relation to [Apple ARM64 devices](https://en.wikipedia.org/wiki/Apple_silicon).

## Design Philosophy

The objective of this library is to create the fastest multithreaded data structures without sacrificing correctness.

`correctness` $\succ$ `speed`.

<!--
## Entry Template

### `<StructureName>`
- Goal:
- Core representation:
- Concurrency model:
- Promotion/adaptation strategy (if any):
- Memory management strategy:
- API semantics:
- Complexity targets:
- Tunables/constants:
- Tradeoffs accepted:
- Follow-up work: -->

## Structures

### `Stack`

Initial testing showed poor single-threaded performance compared to the STL stack. To improve upon this, a hybrid design was implemented such that the stack begins as a vector protected by a spinlock and is later promoted to a lock-free singly-linked list with atomic head [compare-and-swap](https://en.wikipedia.org/wiki/Compare-and-swap) (CAS) upon reaching a threshold of active operations.

A nodal design is used as CAS stack algorithms require stable per-element addresses so threads can atomically swap *only* the head pointer under concurrent `push`/`pop` operations. A linked design gives each node an address and prevents relocation; threads can change the head without moving existing nodes in memory.

[Hazard pointers](https://en.wikipedia.org/wiki/Hazard_pointer) are used in the compare-and-swap mode to allow for safe deffered node deletion, meaning the memory is freed only when no thread contains said node in a hazard slot.

Hazard pointers were included in the implementation as `correctness` $\succ$ `speed`.


### `Queue`

Michael-Scott algorithm (linked list with an atomic head/tail). A ring buffer with a single producer and single consumer would technically be faster, but I would like this implementation to scale to four threads.

Use a ring-buffer if you know the amount of data you must store.


### `Ring Buffer`
