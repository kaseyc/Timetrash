// UCLA CS 111 Lab 1 storage allocation

#include "alloc.h"

#include <error.h>
#include <errno.h>
#include <stdlib.h>

static void
memory_exhausted (int errnum)
{
  error (1, errnum, "memory exhausted");
}

static void *
check_nonnull (void *p)
{
  if (! p)
    memory_exhausted (errno);
  return p;
}

void *
checked_malloc (size_t size)
{
  return check_nonnull (malloc (size ? size : 1));
}

void *
checked_realloc (void *ptr, size_t size)
{
  return check_nonnull (realloc (ptr, size ? size : 1));
}

void *
checked_grow_alloc (void *ptr, size_t *size)
{
  size_t max = -1;
  if (*size == max)
    memory_exhausted (0);
  *size = *size < max / 2 ? 2 * *size : max;
  return checked_realloc (ptr, *size);
}

void free_stream (command_stream_t stream)
{
  int i;

  if (stream == NULL)
    return;

  for(i = 0; i < stream->num_commands; i++)
  {
    free_tree(stream->commands[i]);
  }

  free(stream);
  stream = NULL;
}

void free_tree (command_t root)
{
  char **w;
  if (root == NULL)
    return;

  switch (root->type)
  {
    case AND_COMMAND:
    case OR_COMMAND:
    case PIPE_COMMAND:
    case SEQUENCE_COMMAND:
      free_tree(root->u.command[0]);
      free_tree(root->u.command[1]);
      break;

    case SIMPLE_COMMAND:
      w = root->u.word;
      free(*w);
      while (*++w)
        free(*w);

      free(root->u.word);
      break;

    case SUBSHELL_COMMAND:
      free_tree(root->u.subshell_command);
      break;
  }

  if (root->input != NULL && root->output != NULL)
  {
    free(root->input);
    free(root->output);
    root->input = NULL;
    root->output = NULL;
  }

  free(root);
  root = NULL;
}