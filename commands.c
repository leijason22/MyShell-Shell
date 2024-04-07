#include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <glob.h>

#define MAX_ARGS 64

void wildcards(char ***tokens, int *token_count, int *size) {
    char **expanded_tokens = malloc(MAX_ARGS * sizeof(char*));
    int new_token_count = 0;

    if (!expanded_tokens) {
        fprintf(stderr, "Memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < *token_count; i++) {
        if (strchr((*tokens)[i], '*')) {
            glob_t glob_result;
            if (glob((*tokens)[i], 0, NULL, &glob_result) == 0) {
                for (size_t j = 0; j < glob_result.gl_pathc; j++) {
                    expanded_tokens[new_token_count++] = strdup(glob_result.gl_pathv[j]);
                    if (!expanded_tokens[new_token_count - 1]) {
                        fprintf(stderr, "Memory allocation error\n");
                        exit(EXIT_FAILURE);
                    }
                    if (new_token_count >= MAX_ARGS - 1) {
                        fprintf(stderr, "Too many arguments\n");
                        exit(EXIT_FAILURE);
                    }
                }
                globfree(&glob_result);
            }
        } else {
            expanded_tokens[new_token_count++] = strdup((*tokens)[i]);
            if (!expanded_tokens[new_token_count - 1]) {
                fprintf(stderr, "Memory allocation error\n");
                exit(EXIT_FAILURE);
            }
            if (new_token_count >= MAX_ARGS - 1) {
                fprintf(stderr, "Too many arguments\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Add NULL terminator to the expanded tokens
    expanded_tokens[new_token_count] = NULL;

    // Free the original tokens and update with the expanded tokens
    for (int i = 0; i < *token_count; i++) {
        free((*tokens)[i]);
    }
    free(*tokens);

    *tokens = expanded_tokens;
    *token_count = new_token_count;
    *size = new_token_count;
}

char** parse_command(char *command) {
    const char delimiters[] = " \t\n"; // Tokens are separated by whitespace
    int token_count = 0;
    char *token;
    char **tokens = malloc(MAX_ARGS * sizeof(char*));
    int size = 0; // Current size of the tokens array

    if (!tokens) {
        fprintf(stderr, "Memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(command, delimiters);
    while (token != NULL) {
        tokens[token_count] = strdup(token);
        if (!tokens[token_count]) {
            fprintf(stderr, "Memory allocation error\n");
            exit(EXIT_FAILURE);
        }
        token_count++;
        size++;
        token = strtok(NULL, delimiters);
    }

    // Add NULL terminator to the argument list
    tokens[token_count] = NULL;

    // Expand wildcards
    wildcards(&tokens, &token_count, &size);

    return tokens;
}

// Function to execute a parsed command
void execute_command(char *command, int* flag) {
    //saves current stdout file descriptor, so it can be pointed back to if redirection occurs
    int stdout_backup = dup(STDOUT_FILENO);
    int stdin_backup = dup(STDIN_FILENO);

    char **tokens = parse_command(command);

    // Check for output redirection
    int redirect_output = 0;
    char *redirection_file_output = NULL;
    // Check for input redirection
    int redirect_input = 0;
    char *redirection_file_input = NULL;

    // Loop through tokens to find redirection symbols
    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], ">") == 0) {
            // Output redirection
            redirection_file_output = tokens[i + 1];
            redirect_output = 1;
            break;
        } else if (strcmp(tokens[i], "<") == 0) {
            // Input redirection
            redirection_file_input = tokens[i + 1];
            redirect_input = 1;
            break;
        }
    }

    if (redirect_output) {
        // Open the output file
        int fd = open(redirection_file_output, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if (fd == -1) {
            fprintf(stderr, "Failed to open output file: %s\n", redirection_file_output);
            exit(EXIT_FAILURE);
        }
        // Redirect stdout to the file
        if (dup2(fd, STDOUT_FILENO) == -1) {
            fprintf(stderr, "Failed to redirect output\n");
            close(fd);
            exit(EXIT_FAILURE);
        }
        close(fd);
    }

    // Handle input redirection if needed
    if (redirect_input) {
        // Open the input file
        int fd = open(redirection_file_input, O_RDONLY);
        if (fd == -1) {
            fprintf(stderr, "Failed to open input file: %s\n", redirection_file_input);
            exit(EXIT_FAILURE);
        }
        // Redirect stdin from the file
        if (dup2(fd, STDIN_FILENO) == -1) {
            fprintf(stderr, "Failed to redirect input\n");
            close(fd);
            exit(EXIT_FAILURE);
        }
        close(fd);
    }

    int size = 0;
    while (tokens[size] != NULL) {
        size++;
    }
    // Example: handle built-in commands
    //cd: change the working directory
    //expects one argument, which is a path to a directory
    //use chdir() to change its own directory
    //cd should print an error message and fail if it is given the wrong number of arguments
    //or if chdir() fails
    if(tokens[0] == NULL){} 
    else if ((((strcmp(tokens[0], "then") == 0) && *flag == 1) || ((strcmp(tokens[0], "else") == 0) && *flag == 0)) && tokens[0] != NULL) {
        free(tokens[0]);
        // Shift all elements by one position to the left
        for (int i = 0; i < size - 1; i++) {
            tokens[i] = tokens[i + 1];
        }
        tokens[size - 1] = NULL;
        size--;
    } else if ((((strcmp(tokens[0], "then") == 0) && *flag == 0) || ((strcmp(tokens[0], "else") == 0) && *flag == 1)) && tokens[0] != NULL) {
        printf("Cannot execute because previous statement failed");
        return;
    }

    if(tokens[0] == NULL){} 
    else if (strcmp(tokens[0], "cd") == 0) {
        // Example: chdir(tokens[1]);
        if (tokens[1] == NULL) {
            fprintf(stderr, "cd: missing argument\n");
        } else {
            if (chdir(tokens[1]) != 0) {
                fprintf(stderr, "cd: %s\n", strerror(errno));
            }
            // printf("command executed");
        }
    } else if (strcmp(tokens[0], "pwd") == 0) {
        //pwd: prints current working directory to std output
        //use getcwd()
        // Example: system("pwd");
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            fprintf(stderr, "pwd: %s\n", strerror(errno));
        }
        // printf("command executed");
    } else if (strcmp(tokens[0], "which") == 0) {
        //which: takes a single argument (name of a program), prints path
        //that mysh would use if asked to start that program (result of search for bare names)
        //print nothing and fails if it is given the wrong number of arguments, or the name of a built-in, or if the program
        //is not found
        if (tokens[1] == NULL) {
            fprintf(stderr, "which: missing argument\n");
        } else {
            char *path = getenv("PATH");
            if (path != NULL) {
                char *path_copy = strdup(path);
                char *dir = strtok(path_copy, ":");
                int found = 0; // flag, 0 = not found, 1 = found
                while (dir != NULL) {
                    char command_path[PATH_MAX];
                    snprintf(command_path, sizeof(command_path), "%s/%s", dir, tokens[1]);
                    if (access(command_path, F_OK | X_OK) == 0) {
                        printf("%s\n", command_path);
                        found = 1;
                        break;
                    }
                    dir = strtok(NULL, ":");
                }
                free(path_copy);

                if (!found) {
                    printf("which: %s not found\n", tokens[1]);
                }
            }
        }
    } else if (strcmp(tokens[0], "exit") == 0) {
        //exit: indicates that mysh should cease reading commands and terminate
        // Free memory allocated for tokens
        for (int i = 0; tokens[i] != NULL; i++) {
            free(tokens[i]);
        }
        free(tokens);
        printf("Exiting my shell.\n");
        exit(EXIT_SUCCESS);
    } else {
        // Execute external commands
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            
            char *full_path = NULL;
            // Check if the command is in the current directory
            if (access(tokens[0], F_OK | X_OK) == 0) {
                full_path = strdup(tokens[0]);
            } else {
                // Search for the command in the PATH environment variable
                char *path = getenv("PATH");
                if (path != NULL) {
                    char *path_copy = strdup(path);
                    char *dir = strtok(path_copy, ":");
                    while (dir != NULL) {
                        char command_path[PATH_MAX];
                        snprintf(command_path, sizeof(command_path), "%s/%s", dir, tokens[0]);
                        if (access(command_path, F_OK | X_OK) == 0) {
                            full_path = strdup(command_path);
                            break;
                        }
                        dir = strtok(NULL, ":");
                    }
                    free(path_copy);
                }
            }
            if (full_path != NULL) {
                execv(full_path, tokens);
                // execv returns only if an error occurs
                fprintf(stderr, "Error executing command %s\n", tokens[0]);
                free(full_path);
                exit(EXIT_FAILURE);
            } else {
                fprintf(stderr, "Command not found: %s\n", tokens[0]);
                exit(EXIT_FAILURE);
            }
        } else if (pid < 0) {
            // Fork failed
            fprintf(stderr, "Fork failed\n");
        } else {
            // Parent process
            int status;
            waitpid(pid, &status, 0);
            // Handle exit status if needed
        }
    }
   
    // Restore stdout and stdin to their original file descriptors
    dup2(stdout_backup, STDOUT_FILENO);
    dup2(stdin_backup, STDIN_FILENO);
    close(stdout_backup);
    close(stdin_backup);

    // Free memory allocated for tokens
    for (int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
    }
    free(tokens);

    int status;
    if (WIFEXITED(status)) {
        // The child process terminated normally
        int exit_status = WEXITSTATUS(status);
        if (exit_status != 0) {
            *flag = 0;
        } else {
            *flag = 1;
        }
    } 
}