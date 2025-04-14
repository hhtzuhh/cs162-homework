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
    if (tokens_get_length(tokens) > 2) {
        fprintf(stderr, "cd: too many arguments\n");
        return 1;
    }

    const char* dir;
    if (tokens_get_length(tokens) == 1) {
        // If no argument, change to home directory
        dir = getenv("HOME");
        if (!dir) {
            fprintf(stderr, "cd: HOME environment variable not set\n");
            return 1;
        }
    } else {
        dir = tokens_get_token(tokens, 1);
        // Handle ~ expansion
        if (dir[0] == '~') {
            const char* home = getenv("HOME");
            if (!home) {
                fprintf(stderr, "cd: HOME environment variable not set\n");
                return 1;
            }
            if (dir[1] == '\0') {
                dir = home;
            } else if (dir[1] == '/') {
                char* expanded = malloc(strlen(home) + strlen(dir + 1) + 1);
                if (!expanded) {
                    perror("malloc");
                    return 1;
                }
                sprintf(expanded, "%s%s", home, dir + 1);
                dir = expanded;
            }
        }
    }
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

/* Helper function to execute a single command segment */
static int execute_command_segment(struct tokens* tokens, int start, int end, 
                                 int pipe_in, int pipe_out) {
    // Set up input/output redirection
    if (pipe_in != -1) {
        dup2(pipe_in, STDIN_FILENO);
        close(pipe_in);
    }
    if (pipe_out != -1) {
        dup2(pipe_out, STDOUT_FILENO);
        close(pipe_out);
    }

    // Check if it's a built-in command
    int fundex = lookup(tokens_get_token(tokens, start));
    if (fundex >= 0) {
        int status = cmd_table[fundex].fun(tokens);
        exit(status);
    }

    // Handle program execution
    const char* program = tokens_get_token(tokens, start);
    char* full_path = resolve_program_path(program);
    if (!full_path) {
        fprintf(stderr, "%s: command not found\n", program);
        exit(1);
    }

    // Count redirection operators and build argv
    int count = 0;
    int in_fd = -1, out_fd = -1;
    for (int i = start; i < end; i++) {
        char* token = tokens_get_token(tokens, i);
        if (strcmp(token, "<") == 0) {
            ++i;
            if ((in_fd = open(tokens_get_token(tokens, i), O_RDONLY)) < 0) {
                perror("open input file");
                free(full_path);
                exit(1);
            }
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
            count++;
            continue;
        }
        if (strcmp(token, ">") == 0) {
            ++i;
            out_fd = open(tokens_get_token(tokens, i), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd < 0) {
                perror("open output file");
                free(full_path);
                exit(1);
            }
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
            count++;
            continue;
        }
    }

    // Build argv array
    int argc = end - start - (count * 2);
    char* argv[argc + 1];
    argv[0] = (char*)full_path;

    int arg_index = 1;
    for (int i = start + 1; i < end; i++) {
        char* token = tokens_get_token(tokens, i);
        if (!strcmp(token, "<") || !strcmp(token, ">")) {
            i++;
            continue;
        }
        argv[arg_index++] = token;
    }
    argv[arg_index] = NULL;

    execv(full_path, argv);
    perror("execv");
    free(full_path);
    exit(1);
}

/* Helper function to execute a pipeline of commands */
static int execute_pipeline(struct tokens* tokens) {

    // Count number of commands (number of pipes + 1)
    int num_commands = 1;
    for (int i = 0; i < tokens_get_length(tokens); i++) {
        if (strcmp(tokens_get_token(tokens, i), "|") == 0) {
            num_commands++;
        }
    }
    // Check if it's a built-in command (no pipes)
    if (num_commands == 1) {
        int fundex = lookup(tokens_get_token(tokens, 0));
        if (fundex >= 0) {
            int status = cmd_table[fundex].fun(tokens);
            // If it's exit command, exit the entire shell
            if (strcmp(cmd_table[fundex].cmd, "exit") == 0) {
                exit(0);
            }
            return status;
        }
    }



    // Create pipes
    int (*pipes)[2] = malloc((num_commands - 1) * sizeof(int[2]));
    if (!pipes) {
        perror("malloc");
        return -1;
    }

    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            free(pipes);
            return -1;
        }
    }

    // Fork and execute commands
    int start = 0;
    int segment = 0;
    pid_t* pids = malloc(num_commands * sizeof(pid_t));
    if (!pids) {
        perror("malloc");
        free(pipes);
        return -1;
    }

    for (int i = 0; i <= tokens_get_length(tokens); i++) {
        if (i == tokens_get_length(tokens) || 
            strcmp(tokens_get_token(tokens, i), "|") == 0) {
            pids[segment] = fork();
            if (pids[segment] == 0) {
                // Child process
                int pipe_in = (segment > 0) ? pipes[segment-1][0] : -1;
                int pipe_out = (segment < num_commands-1) ? pipes[segment][1] : -1;
                
                // Close pipe ends in child, only preserve the needed one in this child process
                for (int j = 0; j < num_commands - 1; j++) {
                    if (j != segment - 1) close(pipes[j][0]); // close read end
                    if (j != segment) close(pipes[j][1]); // close write end
                }

                execute_command_segment(tokens, start, i, pipe_in, pipe_out);
            } else if (pids[segment] < 0) {
                perror("fork");
                free(pipes);
                free(pids);
                return -1;
            }
            start = i + 1;
            segment++;
        }
    }

    // Parent process: close all pipe ends
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all children
    for (int i = 0; i < num_commands; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0) {
                fprintf(stderr, "Process %d exited with status %d\n", i, exit_status);
            }
        }
    }

    free(pipes);
    free(pids);
    return 0;
}

int main(unused int argc, unused char* argv[]) {
  init_shell();

  static char line[4096];
//   int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
//   if (!shell_is_interactive) 
//     fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);
    
    // Skip empty input
    if (tokens_get_length(tokens) == 0) {
      tokens_destroy(tokens);
      continue;
    }

    // I want to print all the tokens
    // for (int i = 0; i < tokens_get_length(tokens); i++) {
    //   printf("token %d: %s\n", i, tokens_get_token(tokens, i));
    // }

    /* Execute program or pipeline */
    execute_pipeline(tokens);

    // if (!shell_is_interactive)
    //   /* Please only print shell prompts when standard input is not a tty */
    //   fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
