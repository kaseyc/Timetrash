// UCLA CS 111 Lab 1 command execution

#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

int run_command(command_t c);
int build_pipe(command_t pipe_write, command_t pipe_read);
void set_redirects(command_t c);

int
command_status (command_t c)
{
  return c->status;
}

void
execute_command (command_t c, bool time_travel)
{
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */

  run_command(c);
}

int
run_command (command_t c)
{
	pid_t child;
	int status;

	switch (c->type)
	{
		case SIMPLE_COMMAND:
			child = fork();

			if (child == 0)
			{
				set_redirects(c);
				execvp(c->u.word[0], c->u.word);
			}

			else if (child < 0)
				error(1, 0, "Failed to fork process");

			else
			{
				waitpid(child, &status, 0);

				if (WIFEXITED(status))
					status = WEXITSTATUS(status);

				else
					error(1, 0, "Shell command exited with an error");
			}

			break;

		case SUBSHELL_COMMAND:
			set_redirects(c);
			status = run_command(c->u.subshell_command);
			break;

		case AND_COMMAND:
			status = run_command(c->u.command[0]);

			if (status == 0)
				status = run_command(c->u.command[1]);
			break;

		case OR_COMMAND:
			status = run_command(c->u.command[0]);

			if (status != 0)
				status = run_command(c->u.command[1]);
			break;

		case PIPE_COMMAND:
			build_pipe(c->u.command[0], c->u.command[1]);
			break;

		case SEQUENCE_COMMAND:
			run_command(c->u.command[0]);
			status = run_command(c->u.command[1]);
			break;
	}

	return status;
}

int build_pipe(command_t pipe_write, command_t pipe_read)
{
	int fd[2];
	pid_t child, child2;
	int status;

	if (pipe(fd) == -1)
		error(1, errno, "Error opening pipe");

	//Set up the pipe writer
	child = fork();

	if (child < 0)
		error (1, errno, "Error forking process");

	 else if (child == 0)
	 {
	 		if (!pipe_write->output)
	 			dup2(fd[1], 1);
	 		
	 		set_redirects(pipe_write);

	 		close(fd[0]);
	 		close(fd[1]);
	 		execvp(pipe_write->u.word[0], pipe_write->u.word);
	 }

	 //Set up pipe reader
	 child2 = fork();

	 if (child2 < 0)
		error (1, errno, "Error forking process");

	else if (child2 == 0)
	 {
	 		set_redirects(pipe_read);

	 		if (!pipe_read->input)
	 			dup2(fd[0], 0);

	 		close(fd[0]);
	 		close(fd[1]);
	 		execvp(pipe_read->u.word[0], pipe_read->u.word);
	 }

	 close(fd[0]);
	 close(fd[1]);

	 waitpid(child, &status, 0);
	 waitpid(child2, &status, 0);

	 return (WEXITSTATUS(status));
}

void set_redirects(command_t c)
{
	int fd_in, fd_out;
	if (c->input)
	{
		fd_in = open(c->input, O_RDONLY);
		if (fd_in < 0)
			error (1, errno, "Error opening file");

		dup2(fd_in, 0);
	}

	if (c->output)
	{
		fd_out = open(c->output, O_CREAT | O_TRUNC | O_WRONLY, 0644);

		if (fd_out < 0)
			error(1, errno, "Error opening file");

		dup2(fd_out, 1);
	}
}