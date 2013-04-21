// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"
#include "alloc.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <error.h>

char last_char = '\0';
char* error_message = '\0';

bool is_valid_word_char(char c);

command_t get_next_command(int (*get_next_byte) (void *), void *get_next_byte_argument, int *line_num);

void build_command(int (*get_next_byte) (void *), void *get_next_byte_argument, command_t token);

command_stream_t build_command_stream(int (*get_next_byte) (void *), void *get_next_byte_argument, bool in_subshell);

void error_func(command_stream_t stream, command_t current_command, char* message, int line);

command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
  return build_command_stream(get_next_byte, get_next_byte_argument, 0);
}

command_stream_t
build_command_stream(int (*get_next_byte) (void *),
         void *get_next_byte_argument, bool in_subshell)
/* This function reads in commands one by one, and builds parse trees.
   The boolean argument allows the function to modify it's behavior if it
   is evalutating a subshell, allowing the commands to be built recursively */
{
  command_t curr_command, next_command, command_tree, last_operator;
  int commands_size = 10;
  int line_num = 1;
  size_t size;
  command_stream_t stream = (command_stream_t) checked_malloc(sizeof(struct command_stream));
  stream->commands = (command_t*) checked_malloc(commands_size*sizeof(command_t));
  stream->num_commands = 0;
  stream->index = 0;

  curr_command = get_next_command(get_next_byte, get_next_byte_argument, &line_num);

  if (curr_command->status == 1)
    //A value of 1 means an error occurred
    error_func(stream, curr_command, error_message, line_num);

  if (curr_command->type != SIMPLE_COMMAND && curr_command->type != SUBSHELL_COMMAND)
    error_func(stream, curr_command, "Missing left operand", line_num);

  command_tree = curr_command;

  for(;;)
  {
    next_command = get_next_command(get_next_byte, get_next_byte_argument, &line_num);

    //A return value of NULL means EOF was found.
    //Adds the last command and exits the loop
    if (next_command == NULL)
    {
      if (curr_command->type != SIMPLE_COMMAND && curr_command->type != SUBSHELL_COMMAND && curr_command->type != SEQUENCE_COMMAND)
        error_func(stream, curr_command, "Missing operator", line_num);

      if (command_tree != NULL)
      {
        stream->commands[stream->num_commands] = command_tree;
        stream->num_commands++;
      }

      break;
    }

    /* Builds the parse tree based on current token, and the next one we find
       Each element is added to the tree as it is found, and the tree grows in an
       upward-rightward direction

       Becuase we know that last operator that has not had it's right side filled,
       higher precedence operators can be added without backtracking using pointer swaps */

    switch(next_command->type)
    {
      case SEQUENCE_COMMAND:
        if (curr_command->type != SIMPLE_COMMAND && curr_command->type != SUBSHELL_COMMAND)
          error_func(stream, curr_command, "Missing operand", line_num);

        if (in_subshell)
        {
          next_command->u.command[0] = command_tree;
          command_tree = next_command;
          last_operator = next_command;
        }

        else
        {
          stream->commands[stream->num_commands] = command_tree;
          stream->num_commands++;
          command_tree = NULL;

          if (stream->num_commands == commands_size)
          {
            commands_size *= 2;
            size = commands_size*sizeof(command_t);
            stream->commands = (command_t *) checked_grow_alloc(stream->commands, &size);
            commands_size = size/sizeof(command_t);
          }
        }
        break;

      case AND_COMMAND:
      case OR_COMMAND:
        if (curr_command->type != SIMPLE_COMMAND && curr_command->type != SUBSHELL_COMMAND)
          error_func(stream, curr_command, "Missing operand", line_num);

        if (in_subshell)
        {
          last_operator = next_command;

          if (command_tree->type != SEQUENCE_COMMAND)
          {
            next_command->u.command[0] = command_tree;
            command_tree = next_command;
          }

          else
          {
            next_command->u.command[0] = command_tree->u.command[1];
            command_tree->u.command[1] = next_command;
          }
        }

        else
        {
          next_command->u.command[0] = command_tree;
          command_tree = next_command;
        }
        break;

      case PIPE_COMMAND:

        if (curr_command->type != SIMPLE_COMMAND && curr_command->type != SUBSHELL_COMMAND)
          error_func(stream, curr_command, "Missing operand", line_num);

        if (in_subshell)
        {
          if(command_tree->type == PIPE_COMMAND || command_tree->type == SUBSHELL_COMMAND || command_tree->type == SIMPLE_COMMAND)
          {
            next_command->u.command[0] = command_tree;
            command_tree = next_command;
          }

          else
          {
            next_command->u.command[0] = last_operator->u.command[1];
            last_operator->u.command[1] = next_command;
          }
        }

        else
        {
          if (command_tree->type == PIPE_COMMAND || command_tree->type == SUBSHELL_COMMAND || command_tree->type == SIMPLE_COMMAND)
          {
            next_command->u.command[0] = command_tree;
            command_tree = next_command;
          }

          else
          {
            next_command->u.command[0] = command_tree->u.command[1];
            command_tree->u.command[1] = next_command;
          }
        }
        break;

      case SIMPLE_COMMAND:
      case SUBSHELL_COMMAND:
        if (curr_command->type == SIMPLE_COMMAND || curr_command->type == SUBSHELL_COMMAND)
          error_func(stream, curr_command, "Missing operator", line_num);

        else if (command_tree == NULL)
          command_tree = next_command;

        else
          curr_command->u.command[1] = next_command;
        break;
    }

    curr_command = next_command;
  }

  return stream;
}

/* Returns the next command from the stream, or NULL is the end is reached */
command_t
read_command_stream (command_stream_t s)
{
  command_t val;

  if (s == NULL || (s->index >= s->num_commands))
    return NULL;

  val = s->commands[s->index];
  s->index++;

  return val;
}

/* Returns true if a a word can
   contain the character */
bool
is_valid_word_char(char c)
{
  char *acceptable_chars = "!%+,-./:@^_";

  return (isascii(c) && (isalnum(c) || strchr(acceptable_chars, c)));
}

/* Loops through the stream ignoring trivial whitespace until it finds the
   next command. Returns a pointer to the command, or NULL if EOF is found. */
command_t 
get_next_command(int (*get_next_byte) (void *),
         void *get_next_byte_argument, int* line_num)
{
  char c;
  static enum command_type last_token = SEQUENCE_COMMAND;
  command_t token = (command_t) checked_malloc(sizeof(struct command));
  token->status = -1;

  //Searches for next command
  for(;;)
  {
    c= get_next_byte(get_next_byte_argument);

    if (c == EOF)
      return NULL;

    else if (c == '<' || c == '>')
      //Redirects cannot exist on their own
    {
      error_message = "Missing operand";
      token->status = 1;
      return token;
    }

    else if (c == '#')
    {
      if ( last_char != '\0' && (is_valid_word_char(last_char) || strchr("&|();", last_char)))
      {
      error_message = "Invalid character";
      token->status = 1;
      return token;
      }

      while (c != '\n')
      {
        c = get_next_byte(get_next_byte_argument);
        if (c == EOF)
          return NULL;
      }
      ungetc(c, get_next_byte_argument);
    }

    else if (c == '\n')
      //Checks if the newline is significant or not
    {
      *line_num += 1;

      if (last_token == SIMPLE_COMMAND || last_token == SUBSHELL_COMMAND)
        break;

      else
      {
        last_char = c;
        continue;
      }
    }

    else if (c == ' ' || c == '\t')
    {
      last_char = c;
      continue;
    }

    else
      break;
  }

  last_char = c;


  //Determines the command type
  switch (c)
  {
    case '&':
 
      if (c != '&')
      {
      error_message = "Invalid operator";
      token->status = 1;
      return token;
      }

      last_token = AND_COMMAND;
      last_char = c;
      c = get_next_byte(get_next_byte_argument);

      token->type = AND_COMMAND;
      token->u.command[0] = NULL;
      token->u.command[1] = NULL;
      break;

    case '|': 

      last_char = c;
      c = get_next_byte(get_next_byte_argument);

      if (c != '|')
      {
        token->type = PIPE_COMMAND;
        last_token = PIPE_COMMAND;
        last_char = '|';
        ungetc(c, get_next_byte_argument);
      }

      else
      {
        token->type = OR_COMMAND;
        last_token = OR_COMMAND;
      }

      token->u.command[0] = NULL;
      token->u.command[1] = NULL;
      break;

    case '\n':
    case ';':

      last_token = SEQUENCE_COMMAND;
      token->type = SEQUENCE_COMMAND;
      token->u.command[0] = NULL;
      token->u.command[1] = NULL;
      break;

    case '(':
    case ')':
      last_token = SUBSHELL_COMMAND;
      token->type = SUBSHELL_COMMAND;
      break;

    default:
      last_token = SIMPLE_COMMAND;
      token->type = SIMPLE_COMMAND;
      ungetc(c, get_next_byte_argument);
      break;
  }

  if (token->type == SIMPLE_COMMAND || token->type == SUBSHELL_COMMAND)
    build_command(get_next_byte, get_next_byte_argument, token);

  return token;
}

/* Parses redirects for simple commands and subshells
   and creates the word array for simple commands 
   Subshells are built recurcively using build_command_stream */ 
void
build_command(int (*get_next_byte) (void *), void *get_next_byte_argument, command_t token)
{
  int buf_size = 100, buf2_size = 50;
  int words_size = 4;
  int buf_index = 0, buf2_index = 0, words_index = 0;
  int nesting_level = 1;
  int input_chars = 0, output_chars = 0;
  size_t size;
  char *buf = (char*) checked_malloc(buf_size);
  char *buf2;
  char **words = (char**) checked_malloc(words_size * sizeof(char*));
  char *special_chars = "&|()#;\n";
  char *in_pos, *out_pos, *i;
  char c;
  command_stream_t subshell_tree;

  switch (token->type)
  {
    case SIMPLE_COMMAND:
      //Look for words separated by whitespace
      //Stop once a special character or EOF is found
      for(;;)
      {
        c = get_next_byte(get_next_byte_argument);
        if (strchr(special_chars, c) || c == '<' || c == '>' || c == EOF)
        {
          if(buf_index != 0)
          {
            buf[buf_index] = '\0';
            words[words_index] = buf;
            words_index++;
          }
          break;
        }

        else if (c == ' ' || c == '\t')
        {
          if(buf_index != 0)
          {
            buf[buf_index] = '\0';
            words[words_index] = buf;
            buf = (char*) checked_malloc(buf_size);
            buf_index = 0;
            words_index++;

            if (words_index == words_size)
            {
              words_size *= 2;
              size = (words_size*sizeof(char*));
              words = (char**) checked_grow_alloc(words, &size);
              words_size = size/sizeof(char*);
            }
          }
        }

        else if (is_valid_word_char(c))
        {
          buf[buf_index] = c;
          buf_index++;

          if (buf_index == buf_size-1)
          {
            buf_size *= 2;
            size = buf_size;
            buf = (char*) checked_grow_alloc(buf, &size);
            buf_size = size;
          }
        }

        else
        {
          error_message = "Invalid character";
          token->status = 1;
          free(buf);
          token->u.word = words;

          return;
        }
      }


      token->u.word = words;
      break;

    case SUBSHELL_COMMAND:
    /* Look through until the matching end parentheses or EOF  is found */
      for (;;)
      {
        c = get_next_byte(get_next_byte_argument);
        if (c == EOF)
        {
          error_message = "Mismatched parentheses";
          token->status = 1;
          free(buf);
          return;
        }

        else
        {
          if (c == '(')
            nesting_level++;

          if (c == ')')
          {
            nesting_level--;

            if (nesting_level < 0)
            {
              error_message = "Mismatched parentheses";
              token->status = 1;
              free(buf);
              return;
            }

            if (nesting_level == 0)
            {
              c = get_next_byte(get_next_byte_argument);
              break;
            }
          }

          buf[buf_index] = c;
          buf_index++;

          if (buf_index == buf_size)
          {
            buf_size *= 2;
            size = buf_size;
            buf = (char*) checked_grow_alloc(buf, &size);
            buf_size = size;
          }
        }
      }

      //Makes a recursive call to make_command_stream to generate the
      //command tree for the subshell
      FILE *subshell_stream = fmemopen(buf, buf_index, "r");
      if (! subshell_stream)
      {
        error_message = "Unable to open stream";
        token->status = 1;
        free(buf);
        return;
      }

      subshell_tree = build_command_stream(get_next_byte, subshell_stream, 1);
      token->u.subshell_command = subshell_tree->commands[0];
      break;

    default:
      return;
  }

  //Reads the location of redirects into buf, stripping whitespace
  buf = (char*) checked_malloc(buf_size);
  buf_index = 0;
  while (c != EOF)
  {
    if (!strchr(special_chars, c))
    {
      if (is_valid_word_char(c) || c =='<' || c == '>')
      {

        if (c == '<')
          input_chars++;

        if (c == '>')
          output_chars++;

        if (input_chars > 1 || output_chars > 1)
        {
          error_message = "Invalid redirect";
          token->status = 1;
          free(buf);
          return;
        }

        buf[buf_index] = c;
        buf_index++;

        if (buf_index == buf_size - 1)
        {
          buf_size *= 2;
          size = buf_size;
          buf = (char*) checked_grow_alloc(buf, &size);
          buf_size = size;
        }
      }

      else if (c != ' ' && c != '\t')
      //Returns an error is there is a non-whitespace character 
      //that is not a redirect or valid word character.
      {
        error_message = "Invalid character";
        token->status = 1;
        free(buf);
        return;
      }

      last_char = c;
      c = get_next_byte(get_next_byte_argument);
    }

    else
    {
      buf[buf_index] = '\0';
      buf_index++;
      ungetc(c, get_next_byte_argument);
      break;
    }
  }

  //Sets the I/O redirects
  token->input = NULL;
  token->output = NULL;

  in_pos = strchr(buf, '<');
  out_pos = strchr(buf, '>');

  if (in_pos && out_pos)
  {
    if (out_pos < in_pos)
      {
        error_message = "Output must come after input";
        token->status = 1;
        free(buf);
        return;
      }

    buf2 = checked_malloc(buf2_size);
    for (i = in_pos+1; i != out_pos; i++)
    {
      if (*i == '<')
      {
        error_message = "Invalid redirect";
        token->status = 1;
        free(buf);
        free(buf2);
        return;
      }

      buf2[buf2_index] = *i;
      buf2_index++;

      if (buf2_index == buf2_size-1)
      {
        buf2_size *= 2;
        size = buf2_size;
        buf = (char*) checked_grow_alloc(buf, &size);
        buf2_size = size;
      }
    }

    buf2[buf2_index] = '\0';
    if (buf2[0] == '\0')
      {
        error_message = "Invalid redirect";
        token->status = 1;
        free(buf);
        free(buf2);
        return;
      }

    token->input = buf2;

    buf2_size = 25;
    buf2_index = 0;
    buf2 = checked_malloc(buf2_size);

    for (i = out_pos+1; *i != '\0'; i++)
    {
      if (*i == '<' || *i == '>')
      {
        error_message = "Invalid redirect";
        token->status = 1;
        free(buf);
        free(buf2);
        return;
      }

      buf2[buf2_index] = *i;
      buf2_index++;

      if (buf2_index == buf2_size-1)
      {
        buf2_size *= 2;
        size = buf2_size;
        buf = (char*) checked_grow_alloc(buf, &size);
        buf2_size = size;
      }
    }

    buf2[buf2_index] = '\0';
    if (buf2[0] == '\0')
      {
        error_message = "Invalid redirect";
        token->status = 1;
        free(buf);
        free(buf2);
        return;
      }

    token ->output = buf2;
    free (buf);
  }


  else if (in_pos)
  {
    token->input = strtok(buf, "<");
    if (token->input == NULL)
      {
        error_message = "Invalid redirect";
        token->status = 1;
      }
  }

  else if (out_pos)
  {
    token->output = strtok(buf, ">");
    if (token->output == NULL)
      {
        error_message = "Invalid redirect";
        token->status = 1;
      }
    }
}

void error_func(command_stream_t stream, command_t current_command, char* message, int line)
{
  free_stream(stream);
  free_tree(current_command);

  error(1, 0, "%d: %s", line, message);
}