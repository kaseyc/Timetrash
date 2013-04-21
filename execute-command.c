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

pid_t run_command(command_t c);
command_t find_command(command_t c, bool side);
void set_IO(command_t c);
pid_t execute (command_t c);

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

     pid_t pid;
     int status;
     pid = run_command(c);
     if (c->type == SIMPLE_COMMAND)
     	waitpid(pid, &status, 0);
}

pid_t
run_command (command_t c)
{
	pid_t pid, pid2;
	int status;
	int fd[2];

	switch (c->type)
	{
		case SIMPLE_COMMAND:
			pid = execute(c);
			break;

		case SUBSHELL_COMMAND:
			set_IO(c);
			pid = run_command(c->u.subshell_command);
			if (c->u.subshell_command->type == SIMPLE_COMMAND)
			{
				waitpid(pid, &status, 0);
				c->u.subshell_command->status = WEXITSTATUS(status);
			}

			c->status = c->u.subshell_command->status;
			break;

		case AND_COMMAND:
			pid = run_command(c->u.command[0]);

			if (c->u.command[0]->type == SIMPLE_COMMAND)
			{
				waitpid(pid, &status, 0);
				c->u.command[0]->status = WEXITSTATUS(status);
			}

			c->status = c->u.command[0]->status;

			if (c->u.command[0]->status == 0)
			{
				pid = run_command(c->u.command[1]);

				if (c->u.command[1]->type == SIMPLE_COMMAND)
				{
					waitpid(pid, &status, 0);
					c->u.command[1]->status = WEXITSTATUS(status);
				}
				c->status = c->u.command[1]->status;
			}
			break;

		case OR_COMMAND:
			pid = run_command(c->u.command[0]);
			if (c->u.command[0]->type == SIMPLE_COMMAND)
			{
				waitpid(pid, &status, 0);
				c->u.command[0]->status = WEXITSTATUS(status);
			}

			c->status = c->u.command[0]->status;

			if (c->u.command[0]->status != 0)
			{
				pid = run_command(c->u.command[1]);
				if (c->u.command[0]->type == SIMPLE_COMMAND)
				{
					waitpid(pid, &status, 0);
					c->u.command[1]->status = WEXITSTATUS(status);
				}
				c->status = c->u.command[1]->status;
			}
			break;

		case PIPE_COMMAND:
			if (pipe(fd) == -1)
				error(1, errno, "Error creating pipe");

			//Sets the pipe locations
			if (c->u.command[0]->type == SIMPLE_COMMAND)
				c->u.command[0]->write_pipe = fd;
			else
				find_command(c->u.command[0], 1)->write_pipe = fd;

			if (c->u.command[1]->type == SIMPLE_COMMAND)
				c->u.command[1]->read_pipe = fd;
			else
				find_command(c->u.command[1], 0)->read_pipe = fd;

			pid = run_command(c->u.command[0]);
			pid2 = run_command(c->u.command[1]);

			close(fd[0]);
			close(fd[1]);

			waitpid(pid, &status, 0);
			c->u.command[0]->status = WEXITSTATUS(status);

			waitpid(pid2, &status, 0);
			c->u.command[1]->status = WEXITSTATUS(status);
			c->status = WEXITSTATUS(status);

			break;

		case SEQUENCE_COMMAND:
			pid = run_command(c->u.command[0]);
			if (c->u.command[0]->type == SIMPLE_COMMAND)
			{
				waitpid(pid, &status, 0);
				c->u.command[0]->status = WEXITSTATUS(status);
			}

			pid = run_command(c->u.command[1]);
			if (c->u.command[1]->type == SIMPLE_COMMAND)
			{
				waitpid(pid, &status, 0);
				c->u.command[1]->status = WEXITSTATUS(status);
			}

			c->status = c->u.command[1]->status;

			status = run_command(c->u.command[1]);
			break;
	}

	return pid;
}

command_t find_command(command_t c, bool side)
{
	//Searches through the command tree looking for the lesftmost or rightmost command
	//side == 0 means find the leftmost, side == 1 finds the rightmost
	//Used for piping
	command_t val;

	switch (c->type)
	{
		case AND_COMMAND:
		case OR_COMMAND:
		case SEQUENCE_COMMAND:
		case PIPE_COMMAND:
			val = find_command(c->u.command[side], side);
			break;

		case SUBSHELL_COMMAND:
			val = find_command(c->u.subshell_command, side);
			break;

		default:
			val = c;
			break;
	}

	return val;
}

void
set_IO (command_t c)
{
	int fd_in, fd_out;
	if (c->input)
	{
		fd_in = open(c->input, O_RDONLY);
		if (fd_in < 0)
			error (1, errno, "Error opening file");

		dup2(fd_in, 0);
	}

	if (c->read_pipe)
	{
		if (!c->input)
			dup2(c->read_pipe[0], 0);

		close(c->read_pipe[0]);
		close(c->read_pipe[1]);
	}

	if (c->output)
	{
		fd_out = open(c->output, O_CREAT | O_TRUNC | O_WRONLY, 0644);

		if (fd_out < 0)
			error(1, errno, "Error opening file");

		dup2(fd_out, 1);
	}

	if (c->write_pipe)
	{
		if (!c->output)
			dup2(c->write_pipe[1], 1);
		
		close(c->write_pipe[0]);
		close(c->write_pipe[1]);
	}
}

pid_t
execute(command_t c)
{
	pid_t pid;

	pid = fork();

	if (pid == -1)
		error(1, errno, "Error forking process");

	if (pid == 0)
	{
		set_IO (c);
		execvp(c->u.word[0], c->u.word);
	}

	return pid;
}