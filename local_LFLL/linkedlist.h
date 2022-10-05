/*   
 *   File: linkedlist.h
 *   Author: Vincent Gramoli <vincent.gramoli@sydney.edu.au>, 
 *  	     Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: 
 *   linkedlist.h is part of ASCYLIB
 *
 * Copyright (c) 2014 Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>,
 * 	     	      Tudor David <tudor.david@epfl.ch>
 *	      	      Distributed Programming Lab (LPD), EPFL
 *
 * ASCYLIB is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <atomic_ops.h>

#include "./external/include/common.h"
#include "./external/include/atomic_ops_if.h"
#include "./external/include/utils.h"
#include "./external/include/measurements.h"
#include "./external/include/ssalloc.h"
#include "./external/include/ssmem.h"

#ifdef DEBUG
#define IO_FLUSH                        fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#define DEFAULT_ALTERNATE		0
#define DEFAULT_EFFECTIVE		1

static volatile int stop;
extern __thread ssmem_allocator_t* alloc;

#define TRANSACTIONAL                   4

// why typedef-ing struct can be seen here https://stackoverflow.com/questions/612328/difference-between-struct-and-typedef-struct-in-c/612350#612350
typedef volatile struct node 
{
  skey_t key;
  sval_t val;
  uint8_t padding32[8];
  volatile struct node* next; // here we need to change it to atomic
#if defined(DO_PAD)
  uint8_t padding[CACHE_LINE_SIZE - sizeof(sval_t) - sizeof(skey_t) - sizeof(struct node*)];
#endif
} node_t;

// What is cache aligning?? https://stackoverflow.com/questions/39971639/what-does-cacheline-aligned-mean
typedef ALIGNED(CACHE_LINE_SIZE) struct intset 
{
  node_t *head;
} intset_t;

node_t *new_node(skey_t key, sval_t val, node_t *next, int initializing);
intset_t *set_new();
void set_delete(intset_t *set);