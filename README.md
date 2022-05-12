# Concurrent Data Structure

## Introduction

C language implementation of common concurrent data structures with multiple memory reclamations.

## Schedule

|                                      | BUILD | TESTED | Reclamation |
| ------------------------------------ | :---: | :----: | :---------: |
| QSBR                                 |   O   |   O    |     --      |
| EBR                                  |   O   |   O    |     --      |
| HPBR                                 |   O   |   O    |     --      |
| lazy-sync linked list                |   O   |   O    |      X      |
| lock-free linked list                |   O   |   O    |      X      |
| producer-consumer queue              |   O   |   O    |      X      |
| lazy-sync queue                      |   O   |   O    |      X      |
| lock-free back-off stack             |   O   |   O    |      X      |
| lock-free elimination back-off stack |   O   |   O    |      X      |
| lock-free hashset                    |   O   |   O    |      X      |
| concurrent heap                      |   O   |   O    |      X      |
| lazy-sync skiplist                   |   O   |   O    |      X      |
| lock-free skiplist                   |   O   |   O    |      X      |
| concurrent b+tree                    |   O   |   O    |     --      |

* Concurrent Data Structures:

All these three data structures are available to build and tested.

You are free to "play" with it. 

* Memory Reclamation:

**QSBR**: quiescent-state-based reclamation, **EBR**: epoch-based reclaimation, **HPBR**: hazard-pointer-based reclaimation

All these three reclamations are available to build and tested, use them as you wish.

All these data structures use **EBR** by default(I try to use ```#ifdef``` to group them together, but then the code is too ugly to read, so I change my mind). 

You can change to other reclamation by adding code where there are TODO comments.


## References

The art of multiprocessor programming

O. Shalev and N. Shavit. Split-ordered lists: lock-free extensible hash tables

P. E. McKenney and J. D. Slingwine. Read-copy update: Using execution history to solve concurrency problems

K. Fraser. Practical lock-freedom

Michael M M. Hazard pointers: Safe memory reclamation for lock-free objects

https://15445.courses.cs.cmu.edu/fall2018/slides/09-indexconcurrency.pdf