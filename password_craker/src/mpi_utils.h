#ifndef MPI_UTILS_H
#define MPI_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "mpi.h"

#define TAG_PASSWORD          0
#define TAG_INTERVAL          1
#define TAG_INTERVAL_REQUEST  2
#define TAG_KILL_SELF         3
#define TAG_NO_MORE_INTERVAL  4

typedef struct {
    unsigned long long start;
    unsigned long long num_perms;
} Interval;

void createMPIDatatype_Interval(MPI_Datatype* type){
    const int nitems=2;
    int          blocklengths[2] = {1,1};
    MPI_Datatype types[2] = {MPI_UNSIGNED_LONG_LONG, MPI_UNSIGNED_LONG_LONG};
    MPI_Aint     offsets[2];

    offsets[0] = offsetof(Interval, start);
    offsets[1] = offsetof(Interval, num_perms);

    MPI_Type_create_struct(nitems, blocklengths, offsets, types, type);
    MPI_Type_commit(type);
}

#endif