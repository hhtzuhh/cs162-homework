#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t* fun;
  char* cmd;
  char* doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "print current working directory"},
    {cmd_cd, "cd", "change current working directory"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

/* Prints current working directory */
int cmd_pwd(unused struct tokens* tokens) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
        return 0;  // Success
    } else {
        perror("pwd");
        return 1;  // Failure
    }
}

/* Changes current working directory */
int cmd_cd(struct tokens* tokens) {
  // length can only be 2? what if it's more than 2?
  // and other cd functions
    if (tokens_get_length(tokens) != 2) {
        fprintf(stderr, "cd: missing directory argument\n");
        return 1;  
    }
    // TODO:handle ~,and check some other cases, check online
    const char* dir = tokens_get_token(tokens, 1);
    if (chdir(dir) != 0) {
        perror("cd");
        return 1;  
    }
    return 0;
}

/* Helper function to check if a file exists and is executable */
static bool is_executable(const char* path) {
    struct stat st;
    return access(path, X_OK) == 0 &&
           stat(path, &st) == 0 &&
           S_ISREG(st.st_mode);  // Ensures it's a regular file
}

/* Helper function to resolve program path using PATH environment variable */
static char* resolve_program_path(const char* program) {
    // what is this?
    if (strchr(program, '/')) {
        if (is_executable(program)) {
            return strdup(program);
        }
        return NULL;
    }

    // Get PATH environment variable
    const char* path_env = getenv("PATH");
    if (!path_env) {
        return NULL;
    }
    printf("path string: %s\n", path_env);

    // Make a copy of PATH for strtok_r
    char* path_copy = strdup(path_env);
    if (!path_copy) {
        return NULL;
    }
    // sequential search the path var, and try to put the program into path
    // and test if is_executable
    char* saveptr;
    char* dir = strtok_r(path_copy, ":", &saveptr);
    char* full_path = NULL;

    while (dir) {
        // Allocate space for dir + / + program + null terminator
        size_t full_path_len = strlen(dir) + 1 + strlen(program) + 1;
        full_path = malloc(full_path_len);
        if (!full_path) {
            free(path_copy);
            return NULL;
        }

        // Construct full path
        snprintf(full_path, full_path_len, "%s/%s", dir, program);

        // Check if file exists and is executable
        if (is_executable(full_path)) {
            free(path_copy);
            return full_path;
        }

        free(full_path);
        full_path = NULL;
        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);
    return NULL;
}

int main(unused int argc, unused char* argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (!shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);
    
    // Skip empty input
    if (tokens_get_length(tokens) == 0) {
      tokens_destroy(tokens);
      continue;
    }

    // I want to print all the tokens
    for (int i = 0; i < tokens_get_length(tokens); i++) {
      printf("token %d: %s\n", i, tokens_get_token(tokens, i));
    }

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* Execute program */
      const char* program = tokens_get_token(tokens, 0);
      char* full_path = resolve_program_path(program);
      
      if (!full_path) {
        fprintf(stderr, "%s: command not found\n", program);
        tokens_destroy(tokens);
        continue;
      }
      //for loop the tokens_get_token from index 1
      // and check if contain <, and >. and chec
      pid_t pid = fork();
      if (pid == 0) {
        // Child process
        char *token_per;
        int count = 0;
        int in = -1, out = -1;
        // define in and out
        for (int i = 0; i < tokens_get_length(tokens); i++) {
          token_per = tokens_get_token(tokens, i);
          if (strcmp(token_per, "<") == 0) {
            ++i;
            if ((in = open(tokens_get_token(tokens, i), O_RDONLY)) < 0) {
              perror("open input file");
              free(full_path);
              exit(1);
            }
            dup2(in, STDIN_FILENO);         // duplicate stdin to input file
            close(in);                      // close after use
            count++;
            continue;
          }

          if (strcmp(token_per, ">") == 0) {
            ++i;
            out = open(tokens_get_token(tokens, i), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out < 0) {
              perror("open output file");
              free(full_path);
              exit(1);
            }
            dup2(out, STDOUT_FILENO);       // redirect stdout to file
            close(out);                     // close after use
            count++;
            continue;
          }
        }

        count = count*2; // operator , ">", and the file should not in args
        // Convert tokens to argv array
        int argc = tokens_get_length(tokens);
        argc = argc - count;
        char* argv[argc + 1];  // +1 for NULL terminator
        
        // First argument is the full path
        argv[0] = full_path;



        // Copy remaining arguments, and skip
        int arg_index = 1;
        for (int i = 1; i < argc; i++) {
          char *tok = tokens_get_token(tokens, i);
          if (!strcmp(tok, "<") || !strcmp(tok, ">")) {
            i++;
            continue;
          }
          argv[arg_index++] = tok;
        }
        argv[arg_index] = NULL;  // NULL terminate the array
        
        // Execute the program
        execv(full_path, argv);
        
        // If we get here, execv failed
        perror("execv");
        free(full_path);
        exit(1);
      } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
          int exit_status = WEXITSTATUS(status);
          if (exit_status != 0) {
            fprintf(stderr, "Program exited with status %d\n", exit_status);
          }
        }
        free(full_path);
      } else {
        // Fork failed
        perror("fork");
        free(full_path);
      }
    }

    if (!shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
