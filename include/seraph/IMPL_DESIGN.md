# Seraph Data Structure Design Choices

This document outlines design choices for each of the data structures. It mainly discusses the memory models and locking mechanisms with relation to Apple ARM64 devices.

## Design Philosophy

The objective of this library is to create the fastest multithreaded data structures without sacrificing correctness.

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

Initial testing showed poor single-threaded performance in comparison to the STL stack. To correct this, a hybrid design was implemented such that the stack begins as a vector protected by a spinlock and is later promoted to a lock-free singly linked list with atomic head [compare-and-swap](https://en.wikipedia.org/wiki/Compare-and-swap) upon reaching a threshold of active operations.

[Hazard pointers](https://en.wikipedia.org/wiki/Hazard_pointer) are used in the compare-and-swap mode to allow for safe deffered node deletion, meaning the memory is freed only when no thread has said node in a hazard slot.

Hazard pointers were included in the implementation as `correctness` $\succsim$ `speed`.
