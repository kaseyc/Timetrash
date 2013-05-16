// UCLA CS 111 Lab 1 storage allocation
#include <stddef.h>
#include <stdlib.h>
#include "command.h"
#include "command-internals.h"
void *checked_malloc (size_t);
void *checked_realloc (void *, size_t);
void *checked_grow_alloc (void *, size_t *);
void free_stream(command_stream_t stream);
void free_tree(command_t root);
