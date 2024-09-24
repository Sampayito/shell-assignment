// The MIT License (MIT)
// 
// Copyright (c) 2024 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h> //printf(), fgets()
#include <unistd.h> //fork(), execv()
#include <sys/wait.h> //waitpid()
#include <stdlib.h> //malloc(), free(), exit()
#include <errno.h>
#include <string.h> //strlen(), strcpy(), strcmp()

#define WHITESPACE " \t\n" //defines delimiters when splitting command line
#define MAX_COMMAND_SIZE 255
#define MAX_NUM_ARGUMENTS 12
#define MAX_PATH 4096

int main(int argc, char* argv[] )
{

  char* command_string = (char*)malloc(MAX_COMMAND_SIZE); //holds user's input command
  char error_message[30] = "An error has occured\n";

  while(1)
  {
    printf ("msh> "); //prints out the msh prompt

    //reads the command from the command line
    //the while command will wait here until the user inputs something
    if (!fgets(command_string, MAX_COMMAND_SIZE, stdin))
    {
      break; //reached EOF
    }

    command_string[strcspn(command_string, "\n")] = '\0'; //replaces newline with null terminator

    if (strlen(command_string) == 0) //skip empty input lines and prompt user again
    {
      continue;
    }

    ///* Parse input *///
    char *token[MAX_NUM_ARGUMENTS]; //holds commands and arguments (tokens)

    int token_count = 0;                                 

    char *argument_pointer; //pointer to point to token parsed by strsep                                  
                                                           
    char *working_string = strdup(command_string); //duplicates string to modify copy        

    //we are going to move the working_string pointer to
    //keep track of its original value so we can deallocate
    //the correct amount at the end
    
    char *head_ptr = working_string; //used to free working_string at later point
    
    //strsep() splits working_string into tokens based on delimiters
    //each call to strsep() updates working_string to point to the next part of the string
    while (((argument_pointer = strsep(&working_string, WHITESPACE)) != NULL) && //while more tokens
              (token_count < MAX_NUM_ARGUMENTS - 1)) //reserving space for the NULL terminator
    {
      //token[token_count] = strndup( argument_pointer, MAX_COMMAND_SIZE );
      //if( strlen( token[token_count] ) == 0 )
      if(strlen(argument_pointer) > 0) //skip tokens that might result from consecutive delimiters
      //argument_pointer contains token obtained by strsep()
      {
        token[token_count] = strdup(argument_pointer); //duplicate token and store in token array
        //token[token_count] = NULL;
        token_count++;
      }
        //token_count++;
    }
    token[token_count] = NULL; //has to be NULL terminated for execvp to work

////////////////////////////////////////////////////////////////////////////
    // Now print the tokenized input as a debug check
    int token_index  = 0;
    for( token_index = 0; token_index < token_count; token_index ++ ) 
    {
      printf("token[%d] = %s\n", token_index, token[token_index] );  
    }
///////^^this just prints tokens////////////////////////////////////////////

    if (token_count == 0) //skip if no tokens found and prompt user again
    {
      free(head_ptr);
      continue;
    }

    if (strcmp(token[0], "exit") == 0 || strcmp(token[0], "quit") == 0)
    {
      if (token_count != 1)
      {
        write(STDERR_FILENO, error_message, strlen(error_message));
      }
      else
      {
        free(head_ptr);
        free(command_string);
        exit(0);
      }
      for (int i = 0; i < token_count; i++)
      {
        free(token[i]);
      }
      continue;
    }
    else if (strcmp(token[0], "cd") == 0)
    { //fix cd with single argument
      if (token_count != 2) //expects one arg with cd
      {
        write(STDERR_FILENO, error_message, strlen(error_message));
      }
      else
      {
        if (chdir(token[1]) != 0) //change directory
        {
          write(STDERR_FILENO, error_message, strlen(error_message)); //directory does not exist
        }
      }
      //free memory and prompt user
      free(head_ptr);
      for (int i = 0; i < token_count; i++)
      {
        free(token[i]);
      }
      continue; 
    }

    char *path[] = {"/bin/", "/usr/bin/", "/usr/local/bin/", "./"}; //where executable commands are
    char cmd_path[MAX_PATH];
    int found = 0;

    for (int i = 0; i < 4; i++) //loop through each directory in path[]
    {
      //build full path of command by combining directory and command name (ex: /bin/ls for ls)
      snprintf(cmd_path, sizeof(cmd_path), "%s%s", path[i], token[0]);
      if (access(cmd_path, X_OK) == 0) //file exists in dir and is executable if access() returns 0
      {
        found = 1;
        break;
      }
    }

    //if found = 0, command not found so clean memory and prompt user again
    if (!found)
    {
      write(STDERR_FILENO, error_message, strlen(error_message));
      free(head_ptr);
      for (int i = 0; i < token_count; i++)
      {
        free(token[i]);
      }
      continue;
    }

    pid_t child_pid = fork(); //new process created
    int status;

    if (child_pid == -1) //fork failed
    {
      write(STDERR_FILENO, error_message, strlen(error_message));
      free(head_ptr);
      for (int i = 0; i < token_count; i++)
      {
        free(token[i]);
      }
      continue;
    }

    if (child_pid == 0)
    {
      //execv replaces current process with new process
      //take a path to the executable and an array of NULL terminated arguments
      if (execv(cmd_path, token) == -1)
      {
        //could not run executable
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
      }
    }
    else
    {
      waitpid(child_pid, &status, 0); //waits until child changes state
      //status stores the exit status of the child
    }
    
    free(head_ptr); //frees memory allocated by strdup() for working_string
    for (int i = 0; i < token_count; i++) //frees each token duplicated by strdup()
    {
      free(token[i]);
    }

  }
  free(command_string);
  return 0;
}
