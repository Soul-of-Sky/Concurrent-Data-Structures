# Concurrent Data Structures

## Introduction

C language implementation of common concurrent data structures with multiple memory reclamations.

## Schedule

|                                      | BUILD | TESTED | Memory Reclamation | No Memory Leak |
| ------------------------------------ | :---: | :----: | :----------------: | :------------: |
| QSBR                                 |   O   |   O    |         --         |       --       |
| EBR                                  |   O   |   O    |         --         |       --       |
| HPBR                                 |   O   |   O    |         --         |       --       |
| lazy-sync linked list                |   O   |   O    |         O          |       O        |
| lock-free linked list                |   O   |   O    |         O          |       O        |
| producer-consumer(blocking) queue    |   O   |   O    |         --         |       O        |
| lazy-sync queue                      |   O   |   O    |         O          |       O        |
| lock-free back-off stack             |   O   |   O    |         O          |       O        |
| lock-free elimination back-off stack |   O   |   O    |         O          |       O        |
| lock-free hashset                    |   O   |   O    |         O          |       O        |
| concurrent heap                      |   O   |   O    |         --         |       O        |
| lazy-sync skiplist                   |   O   |   O    |         O          |       O        |
| lock-free skiplist                   |   O   |   O    |         O          |       O        |
| concurrent b+tree                    |   O   |   O    |         --         |       O        |

* Concurrent Data Structures:

All these data structures are available to build and tested.

All these data structures are tested with no memory leak using ```valgrind --tool=memcheck``` .

You are free to "play" with it. 

* Memory Reclamation:

**QSBR**: quiescent-state-based reclamation

**EBR**: epoch-based reclaimation

**HPBR**: hazard-pointer-based reclaimation

All these three reclamations are available to build and tested, use them as you wish.

All these data structures use **EBR** by default(I tried to use ```#ifdef``` to group them together, but then the code was too ugly to read, so I changed my mind). 

You can change to other reclamation by adding code in the right place.


## References

The art of multiprocessor programming

O. Shalev and N. Shavit. Split-ordered lists: lock-free extensible hash tables

P. E. McKenney and J. D. Slingwine. Read-copy update: Using execution history to solve concurrency problems

K. Fraser. Practical lock-freedom

Michael M M. Hazard pointers: Safe memory reclamation for lock-free objects

https://15445.courses.cs.cmu.edu/fall2018/slides/09-indexconcurrency.pdf