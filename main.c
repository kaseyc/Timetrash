// UCLA CS 111 Lab 1 main program

#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#include "command.h"
#include "alloc.h"

static char const *program_name;
static char const *script_name;

static void
usage (void)
{
  error (1, 0, "usage: %s [-pt] SCRIPT-FILE", program_name);
}

static int
get_next_byte (void *stream)
{
  int val = getc (stream);

  if (ferror(stream))
    error(1, errno, "Error reading stream");

  return val;
}

int
main (int argc, char **argv)
{
  int command_number = 1;
  bool print_tree = false;
  bool time_travel = false;
  int **dependencies;
  int i, j, status, wait_val;
  pid_t* pids;
  bool can_run, finished;
  command_t* commands;
  program_name = argv[0];

  for (;;)
    switch (getopt (argc, argv, "pt"))
      {
      case 'p': print_tree = true; break;
      case 't': time_travel = true; break;
      default: usage (); break;
      case -1: goto options_exhausted;
      }
 options_exhausted:;

  // There must be exactly one file argument.
  if (optind != argc - 1)
    usage ();

  script_name = argv[optind];
  FILE *script_stream = fopen (script_name, "r");
  if (! script_stream)
    error (1, errno, "%s: cannot open", script_name);
  command_stream_t command_stream =
    make_command_stream (get_next_byte, script_stream);

  if (time_travel)
  {
    commands = command_stream->commands;
    dependencies = set_dependencies(commands, command_stream->num_commands);
    pids = (pid_t*) checked_malloc(sizeof(pid_t) * command_stream->num_commands);
    for (i = 0; i <command_stream->num_commands; i++)
      pids[i] = -1;
  }

  command_t last_command = NULL;
  command_t command;

  if (print_tree || !time_travel)
  {
    while ((command = read_command_stream (command_stream)))
      {
        if (print_tree)
  	{
  	  printf ("# %d\n", command_number++);
  	  print_command (command);
  	}
        else
  	{
  	  last_command = command;
  	  execute_command (command);
  	}
      }
  }

  else
  {
    for(;;)
    {
      finished = true;
      for (i = 0; i < command_stream->num_commands; i++)
      {
        if (pids[i] == -1)
        {
          can_run = true;
          for (j = 0; dependencies[i][j] != -1; j++)
            if (commands[dependencies[i][j]]->status == -1)
                {
                  can_run = false;
                  break;
                }

          if (can_run)
          {
            pids[i] = fork();
            if (pids[i]== -1)
              error(1, errno, "Error forking process");

            if (pids[i] == 0)
            {
              execute_command(commands[i]);
              exit(commands[i]->status);
            }
          }
        }
      }

      for (i = 0; i < command_stream->num_commands; i++)
      {
        if (pids[i] != -1)
        {
          wait_val = waitpid(pids[i], &status, WNOHANG);

          if (wait_val == 0)
            finished = false;

          else
            commands[i]->status = WEXITSTATUS(status);
        }

        else
          finished = false;
      }
      if (finished)
      {
        last_command = commands[command_stream->num_commands-1];
        break;
      }
    }
  }


  free_stream(command_stream);
  if (time_travel)
  {
    free(pids);
    for (i = 0; i<command_stream->num_commands; i++)
      free(dependencies[i]);
    free(dependencies);
  }
  return print_tree || !last_command ? 0 : command_status (last_command);
}
