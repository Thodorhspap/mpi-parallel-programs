# MPI Parallel Programs (C)

Two parallel programs in C using **MPI**, written for the *Introduction to Parallel Computing* course at the University of West Attica. Each program splits an array across processes, works on the parts in parallel and combines the results on process 0.

## 1. `sorted_check.c`: is the array sorted?

Checks whether an array is sorted in ascending order, using only **point-to-point communication** (`MPI_Send` / `MPI_Recv`).

- Process 0 reads the array and sends each process its part. Any remainder goes to the first processes.
- Each process checks its own part.
- Neighbouring processes pass the last value of their part along, so the boundaries between parts are checked too.
- Process 0 collects the results and prints the position of the first element that breaks the order.

```
Enter the size of the array:
6
Type the numbers:
1 2 9 3 5 6
Processor 1 has numbers: 3 5 6
Processor 0 has numbers: 1 2 9
The array is not sorted. First violation at position 4 (value 3 comes after 9).
```

## 2. `vector_stats.c`: vector statistics

Computes statistics of a vector using **collective operations**.

| Result | How it is computed |
|---|---|
| Mean, min, max | Local sums, mins and maxes, combined with `MPI_Reduce` |
| Variance | The mean is shared with `MPI_Bcast`, then local squared deviations are combined with `MPI_Reduce` |
| Vector D | `d_i = (x_i - min) / (max - min) * 100`, so each element becomes its percentage of the range (0 to 100). The parts are collected with `MPI_Gatherv`. |
| Largest D | Its position and the original value |
| Prefix sums | Each process computes local prefix sums. `MPI_Scan` provides each process's starting offset, and `MPI_Gatherv` collects the results. |

The array is distributed with `MPI_Scatterv`, so any N works, including N smaller than the number of processes.

```
please type the length of the array:
8
please type the 8 elements:
3 9 1 7 4 6 2 8
Mean: 5.00
Min: 1
Max: 9
Var: 7.50
New vector D:
25.00
100.00
0.00
75.00
37.50
62.50
12.50
87.50
Max D: 100.00
At i = 2, with xi = 9
Prefix sums:
3 12 13 20 24 30 32 40
```

## Build and run

**Linux / WSL (Open MPI or MPICH):**
```
mpicc -o vector_stats vector_stats.c
mpicc -o sorted_check sorted_check.c
mpiexec -n 4 ./vector_stats
mpiexec -n 4 ./sorted_check
```

**Windows (Microsoft MPI + MinGW gcc):** install MS-MPI and the MS-MPI SDK, then:
```
gcc -o vector_stats.exe vector_stats.c -I"C:\Program Files (x86)\Microsoft SDKs\MPI\Include" -L"C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64" -lmsmpi
mpiexec -n 4 vector_stats.exe
```

## Edge cases handled

- More processes than elements: processes that get no elements are handled correctly
- All elements equal: the D vector is 0 instead of a division by zero
- Invalid input: the program stops with a message
- Large values: prefix sums use 64-bit integers
