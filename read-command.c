// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"
#include "alloc.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <error.h>

struct command_stream
{
  command_t* commands;
  int num_commands;
  int index;
};

char last_char = '\0';

bool is_valid_word_char(char c);

command_t get_next_command(int (*get_next_byte) (void *), void *get_next_byte_argument, int *line_num);

void build_command(int (*get_next_byte) (void *), void *get_next_byte_argument, command_t token, int *line_num);

command_stream_t build_command_stream(int (*get_next_byte) (void *), void *get_next_byte_argument, bool in_subshell);

command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
  return build_command_stream(get_next_byte, get_next_byte_argument, 0);
}

command_stream_t
build_command_stream(int (*get_next_byte) (void *),
         void *get_next_byte_argument, bool in_subshell)
{
  command_t curr_command, next_command, command_tree, last_operator;
  int commands_size = 10;
  int command_index = 0;
  int line_num = 1;
  command_stream_t stream = (command_stream_t) checked_malloc(sizeof(struct command_stream));
  stream->commands = (command_t*) checked_malloc(commands_size*sizeof(command_t));
  stream->index = 0;

  curr_command = get_next_command(get_next_byte, get_next_byte_argument, &line_num);
  if (curr_command->type != SIMPLE_COMMAND && curr_command->type != SUBSHELL_COMMAND)
    error(1, 0, "%d: Missing left operand", line_num);

  command_tree = curr_command;

  for(;;)
  {
    //Get commands and build the parse tree
    next_command = get_next_command(get_next_byte, get_next_byte_argument, &line_num);

    //A return value of NULL means EOF was found.
    //Adds the last command and exits the loop
    if (next_command == NULL)
    {
      if (curr_command->type != SIMPLE_COMMAND && curr_command->type != SUBSHELL_COMMAND && curr_command->type != SEQUENCE_COMMAND)
        error(1,0, "%d: Missing right operand", line_num);

      if (command_tree != NULL)
      {
        stream->commands[command_index] = command_tree;
        command_index++;
        stream->num_commands = command_index;
      }

      break;
    }

    switch(next_command->type)
    {
      case SEQUENCE_COMMAND:
        if (curr_command->type != SIMPLE_COMMAND && curr_command->type != SUBSHELL_COMMAND)
          error(1,0, "%d: Missing left operand", line_num);

        if (in_subshell)
        {
          next_command->u.command[0] = command_tree;
          command_tree = next_command;
          last_operator = next_command;
        }

        else
        {
          stream->commands[command_index] = command_tree;
          command_index++;
          command_tree = NULL;

          if (command_index == commands_size)
          {
            commands_size += 25;
            stream->commands = (command_t *) checked_realloc(stream->commands, commands_size*sizeof(command_t));
          }
        }
        break;

      case AND_COMMAND:
      case OR_COMMAND:
        if (curr_command->type != SIMPLE_COMMAND && curr_command->type != SUBSHELL_COMMAND)
          error(1,0, "%d: Missing left operand", line_num);

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
          error(1,0, "%d: Missing left operand", line_num);

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
          error(1,0, "%d: Missing operator", line_num);

        else if (command_tree == NULL)
          command_tree = next_command;

        else
          curr_command->u.command[1] = next_command;
        break;
    }

    curr_command = next_command;
  }

  stream->num_commands = command_index;
  return stream;
}

command_t
read_command_stream (command_stream_t s)
{
  command_t val;

  if (s->index >= s->num_commands)
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

command_t 
get_next_command(int (*get_next_byte) (void *),
         void *get_next_byte_argument, int* line_num)
{
  char c;
  static enum command_type last_token = SEQUENCE_COMMAND;
  command_t token = (command_t) checked_malloc(sizeof(struct command));

  //Searches for next command
  for(;;)
  {
    c= get_next_byte(get_next_byte_argument);

    if (c == EOF)
      return NULL;

    else if (c == '<' || c == '>')
      //Redirects cannot exist on their own
      error(1,0, "%d: Invalid redirect", *line_num);

    else if (c == '#')
    {
      if ( last_char != '\0' && (is_valid_word_char(last_char) || strchr("&|();", last_char)))
        error(1, 0, "%d: Invalid character", *line_num);

      while (c != '\n')
      {
        c = get_next_byte(get_next_byte_argument);
        if (c == EOF)
          return NULL;
      }
      ungetc(c, get_next_byte_argument);
    }

    else if (c == '\n')
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
        error(1,0, "%d: Invalid operator", *line_num);

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
    build_command(get_next_byte, get_next_byte_argument, token, line_num);

  token->status = -1;

  return token;
}

/* Parses redirects for simple commands and subshells
   and creates the word array for simple commands */ 
void
build_command(int (*get_next_byte) (void *), void *get_next_byte_argument, command_t token, int* line_num)
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
          error(1,0, "%d: Invalid Character", *line_num);
      }


      token->u.word = words;
      break;

    case SUBSHELL_COMMAND:
      for (;;)
      {
        c = get_next_byte(get_next_byte_argument);
        if (c == EOF)
          error (1,0, "%d: Mismatched Parentheses", *line_num);

        else
        {
          if (c == '(')
            nesting_level++;

          if (c == ')')
          {
            nesting_level--;
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
          error(1, 0, "%d: Invalid redirect", *line_num);

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
        error(1,0, "%d: Invalid Character", *line_num);

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

  token->input = NULL;
  token->output = NULL;

  if (buf_index == 2)
    error (1, 0, "%d: Invalid redirect", *line_num);

  else
  {
    in_pos = strchr(buf, '<');
    out_pos = strchr(buf, '>');

    if (in_pos && out_pos)
    {
      if (out_pos < in_pos)
        error(1,0, "%d: Output must come after input", *line_num);

      buf2 = checked_malloc(buf2_size);
      for (i = in_pos+1; i != out_pos; i++)
      {
        if (*i == '<')
          error (1, 0, "Invalid Redirect", *line_num);

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

      token->input = buf2;

      buf2_size = 25;
      buf2_index = 0;
      buf2 = checked_malloc(buf2_size);

      for (i = out_pos+1; *i != '\0'; i++)
      {
        if (*i == '<' || *i)

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
        error(1,0, "%d: Redirect is missing argument", *line_num);

      token ->output = buf2;
    }


    else if (in_pos)
      token->input = strtok(buf, "<");

    else if (out_pos)
      token->output = strtok(buf, ">");
  }
}