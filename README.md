# Concurrent Data Structure

## Introduction

C language implementation of common concurrent data structures with multiple memory reclamations

## Schedule

|                                      | BUILD | TESTED | QSBR | EBR  | HPBR |
| ------------------------------------ | :---: | :----: | :--: | :--: | :--: |
| QSBR                                 |   X   |   X    |  --  |  --  |  --  |
| EBR                                  |   X   |   X    |  --  |  --  |  --  |
| HPBR                                 |   X   |   X    |  --  |  --  |  --  |
| lazy-sync linked list                |   O   |   O    |  X   |  X   |  X   |
| lock-free linked list                |   O   |   O    |  X   |  X   |  X   |
| producer-consumer queue              |   O   |   O    |  X   |  X   |  X   |
| lazy-sync queue                      |   O   |   O    |  X   |  X   |  X   |
| lock-free back-off stack             |   O   |   O    |  X   |  X   |  X   |
| lock-free elimination back-off stack |   X   |   X    |  X   |  X   |  X   |
| lock-free hashset                    |   X   |   X    |  X   |  X   |  X   |
| cuckoo hashset                       |   X   |   X    |  X   |  X   |  X   |
| concurrent heap                      |   O   |   X    |  X   |  X   |  X   |
| lazy-sync skiplist                   |   O   |   O    |  X   |  X   |  X   |
| lock-free skiplist                   |   O   |   O    |  X   |  X   |  X   |
| concurrent b+tree                    |   X   |   X    |  --  |  --  |  --  |

memory reclamation:

**QSBR**: quiescent-state-based reclamation, **EBR**: epoch-based reclaimation, **HPBR**: hazard-pointer-based reclaimation


## References

The art of multiprocessor programming

P. E. McKenney and J. D. Slingwine. Read-copy update: Using execution history to solve concurrency problems

K. Fraser. Practical lock-freedom

Michael M M. Hazard pointers: Safe memory reclamation for lock-free objects

https://15445.courses.cs.cmu.edu/fall2018/slides/09-indexconcurrency.pdf