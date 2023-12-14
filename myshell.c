#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKENS 100
#define MAX_ALIASES 200
#define MAX_LINE_SIZE 200

typedef struct {
    char alias_name[128];
    char command[256];
} Alias;

Alias aliases[MAX_ALIASES];
int alias_count = 0;

void display_prompt();
void parse_input(char *input, char **tokens);
void execute_command(char **tokens);
void handle_alias(char **tokens);
void display_bello_info();

char last_command[MAX_INPUT_SIZE] = "";
char filename[256] = "aliases";
char current_shell_name[MAX_INPUT_SIZE] = "";

char output_file[MAX_INPUT_SIZE] = "";
int background = 0;
int append = 0;
int output_file_index = 0;
int process_count = 0;

//truncate the quotes from the alias
void truncateFirstIndex(char *array) {
    if (array != NULL && array[0] != '\0') {
        int length = strlen(array);
        for (int i = 0; i < length; ++i) {
            array[i] = array[i + 1];
        }
    }
}
void reverseString(char* str) {
    int length = strlen(str);
    for (int i = 0, j = length - 1; i < j; i++, j--) {
        char temp = str[i];
        str[i] = str[j];
        str[j] = temp;
    }
}

void parse_line(const char* line) {
    char alias_name[128];
    char command[256];

    // Use sscanf to parse the line into alias_name and command
    if (sscanf(line, "%s %[^\n]", alias_name, command) == 2) {
        // Check if there's room in the aliases array
        if (alias_count < MAX_ALIASES) {
            // Copy the values into the aliases array
            strcpy(aliases[alias_count].alias_name, alias_name);
            strcpy(aliases[alias_count].command, command);
            alias_count++;
        } else {
            printf("Warning: Maximum number of aliases reached.\n");
        }
    } else {
        printf("Error parsing line: %s\n", line);
    }
}

//check if the input is an alias and return the command if it is
const char* check_alias(const char* input) {
    for (int i = alias_count-1; i > -1 ; i--) {
        if (strcmp(input, aliases[i].alias_name) == 0) {
            return aliases[i].command;
        }
    }
    return NULL;
}

void current_shell_name_method(char** tokens) {

    for (int i = 0; tokens[i] != NULL; ++i) {
        //free(tokens[i]);
        tokens[i] = NULL;
    }
    //write tokens ps -o args= -p pid
    // calculate it with ps -o args= -p pid 
    // run the command in the child process and read the output in the parent process via pipe

    tokens[0] = strdup("ps");
    tokens[1] = strdup("-o");
    tokens[2] = strdup("args=");
    tokens[3] = strdup("-p");
    //get pid from getppid()
    pid_t piddd = getppid();
    char pppid[10];
    sprintf(pppid, "%d", piddd);
    //write pid to tokens[4]
    tokens[4] = strdup(pppid);
    tokens[5] = NULL;


    int pipe_fd[2];

    pipe(pipe_fd);
    pid_t pid = fork();

    if (pid == -1) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { 
        
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);
        
        execvp(tokens[0], tokens);
        perror("Execution failed");
        exit(EXIT_FAILURE);
            
    } else { // Parent process

        int status;
        waitpid(pid, &status, 0);

        close(pipe_fd[1]);  // Close the write end in the parent

        char buffer[4096];
        //assign buffer to the output of the command

        ssize_t count = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
        
        if (count > 0) {
            buffer[count] = '\0';
            strcpy(current_shell_name, buffer); 
        }
        close(pipe_fd[0]);

    }
}

// Parse the input string into tokens based on whitespace
void parse_input(char *input, char **tokens) {

   
    int token_count = 0;


    char *token = strtok(input, " ");
    while (token != NULL && token_count < MAX_TOKENS - 1) {
        tokens[token_count] = strdup(token);

        if (strcmp(tokens[token_count], "&") == 0) {
            
            background = 1;
            tokens[token_count] = NULL; // Remove "&" from the command
            break; // Background command should not be part of tokens
        }
        //check if there is a direction to a file
        if (strcmp(tokens[token_count], ">>>") == 0) {
            append = 3;
            output_file_index = token_count+1;
        }
        if (strcmp(tokens[token_count], ">>") == 0) {
            append = 2;
            output_file_index = token_count+1;         
        }
        if (strcmp(tokens[token_count], ">") == 0) {
            append = 1;
            output_file_index = token_count+1;   
        }
        
        token = strtok(NULL, " ");
        token_count++;
    }

    tokens[token_count] = NULL; // Null-terminate the array
}
int main() {
    char input[MAX_INPUT_SIZE];
    char *tokens[MAX_TOKENS];
    char alias_command[MAX_INPUT_SIZE];
    
    
    FILE* file = fopen("aliases", "r");
    if (file == NULL) {
        file = fopen(filename, "w");
        //printf("Aliases file is created.");
        //perror("Error opening file");
        //return 1;
    } 

    char line[256];  // Adjust the size according to your needs

    // Read each line from the file
    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove newline character if present
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        // Parse the line and store in the aliases array
        parse_line(line);
    }

    // Close the file
    fclose(file);

    // Get the current shell name
    current_shell_name_method(tokens);

    for (int i = 0; tokens[i] != NULL; ++i) {
        free(tokens[i]);
        tokens[i] = NULL;
    }
   

    while (1) {

        background = 0;
        append = 0;
        strcpy(output_file, "");
        output_file_index = 0;
        

        display_prompt();

        fgets(input, sizeof(input), stdin);

        input[strcspn(input, "\n")] = '\0'; // remove newline character

        if (strlen(input) == 0 || input == NULL) {
            // Empty command, do nothing
            continue;
        }

        parse_input(input, tokens);

        if (strcmp(tokens[0], "exit") == 0) {
            printf("Exiting shell...\n");
            break;
        }

        if (tokens[0] != NULL) {

            if (strcmp(tokens[0], "bello") == 0) {

                // reaping the zombie processes
                pid_t terminated_child;
                do {
                    terminated_child = waitpid(-1, NULL, WNOHANG);
                    /*if (terminated_child > 0) {
                        // A background process has terminated, decrement the count
                        process_count--;
                    }*/
                } while (terminated_child > 0);
                
            }
            //if the first token is alias, call handle_alias to create an alias
            if (strcmp(tokens[0], "alias") == 0) {
                handle_alias(tokens);
            } else {              
                
                char alias_input[MAX_INPUT_SIZE];

                const char* alias_result = check_alias(tokens[0]);
                //if the first token is an alias, replace it with the command
                if (alias_result != NULL) {
                    strcpy(alias_input, alias_result);
                    for (int i = 1; tokens[i] != NULL; ++i) {
                        strcat(alias_input, " ");
                        strcat(alias_input, tokens[i]);
                        
                    }

                    //background = 0;
                    append = 0;
                    strcpy(output_file, "");
                    output_file_index = 0;

                    alias_input[strcspn(alias_input, "\n")] = '\0';

                    parse_input(alias_input, tokens);
                }
                //if there is a direction to a file, assign the file name to output_file and remove it from tokens
                if(append != 0){
                    strcpy(output_file,tokens[output_file_index]);
                    for (int i = output_file_index-1; tokens[i] != NULL; ++i) {
                                    
                        //free(tokens[i]);
                        tokens[i] = NULL;
                    }
                    
                }

                
                execute_command(tokens);

                    
                
            }
            
        }

        // Clean up tokens
        for (int i = 0; tokens[i] != NULL; ++i) {
            free(tokens[i]);
            tokens[i] = NULL;
        }
    }

    return 0;
}


// Display the prompt
void display_prompt() {
    char hostname[128];
    //char username[128];
    char* username = getenv("USER");

    gethostname(hostname, sizeof(hostname));
    //getlogin_r(username, sizeof(username));

    printf("%s@%s %s --- ", username, hostname, getcwd(NULL, 0));
    fflush(stdout);
}


// Execute the command in a fork-exec fashion
void execute_command(char **tokens) {

    int flag = 0;

    //look at the path
    char *path = getenv("PATH");
    char *path_copy = strdup(path);
    char *dir = strtok(path_copy, ":");
    //char *executable_path = NULL;  // Declare outside the loop
    char *success_executable_path = NULL;  // Additional variable


    while (dir != NULL) {
        char *executable_path = malloc(strlen(dir) + strlen(tokens[0]) + 2);
        sprintf(executable_path, "%s/%s", dir, tokens[0]);

        // Check if the executable exists in the current directory
        if (access(executable_path, X_OK) == 0) {
            //if (strcmp(tokens[0], "bello") != 0){
            flag = 1;

            // Copy the successful executable_path to success_executable_path
            success_executable_path = strdup(executable_path);

            strcpy(last_command, "");
            for (int y = 0; tokens[y] != NULL; ++y) {
                strcat(last_command,tokens[y]);
                strcat(last_command," ");
            }
            //printf("\n");
            last_command[strlen(last_command) - 1] = '\0';

            break;

            
        }

        free(executable_path);
        dir = strtok(NULL, ":");
    }

    free(path_copy);

    // Check if the command was found in the path
    if (flag == 0 && strcmp(tokens[0], "bello") != 0) {
        // If the loop completes, the command was not found
        fprintf(stderr, "Command not found: %s\n", tokens[0]);
        return;
    }

    // Fork a child process
    pid_t pid = fork();

    if (pid == -1) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { 
        if (append != 0){

            //file = fopen(output_file, "w");
            if (output_file != NULL) {
                int fd;

                if (append == 2) {
                    // Append output to file
                    fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
                    // Redirect stdout to the file descriptor
                    dup2(fd, STDOUT_FILENO);

                    // Close the file descriptor, as stdout is now pointing to the file
                    close(fd);
                } else if (append == 1) {
                    
                    // Overwrite file with output
                    fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    // Redirect stdout to the file descriptor
                    dup2(fd, STDOUT_FILENO);

                    // Close the file descriptor, as stdout is now pointing to the file
                    close(fd);
                } else if (append == 3) {

                    int pipe_fd[2];

                    pipe(pipe_fd);
                    pid_t pid = fork();

                    if (pid == -1) {
                        perror("Fork failed");
                        exit(EXIT_FAILURE);
                    } else if (pid == 0) { 
                            // Append reversed output to file
                            close(pipe_fd[0]);
                            dup2(pipe_fd[1], STDOUT_FILENO);
                            close(pipe_fd[1]);

                            if (strcmp(tokens[0], "bello") == 0){
                                display_bello_info(tokens);
                                exit(EXIT_SUCCESS);
                            }
                            
                            //printf("Executable path: %s\n", success_executable_path);
                            execv(success_executable_path, tokens);
                            perror("Execution failed");
                            exit(EXIT_FAILURE);
                                
    
                            
                    } else { // Parent of the grandchild process

                        int status;
                        waitpid(pid, &status, 0);

                        close(pipe_fd[1]);  // Close the write end in the parent

                        char buffer[4096];
                        ssize_t count = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
                        if (count > 0) {
                            buffer[count] = '\0';
                            reverseString(buffer);

                            int outFile = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
                            if (outFile == -1) {
                                perror("open");
                                exit(EXIT_FAILURE);
                            }
                            write(outFile, buffer, strlen(buffer));
                            close(outFile);
                        }
                        close(pipe_fd[0]);

                        exit(EXIT_SUCCESS);

                    }
                    
                }

                if (fd == -1) {
                    perror("Error opening file for redirection");
                    exit(EXIT_FAILURE);
                }

            }
        }

        if (strcmp(tokens[0], "bello") == 0){
            display_bello_info(tokens);
            exit(EXIT_SUCCESS);
        }

        //printf("Executable path: %s\n", success_executable_path);

        execv(success_executable_path, tokens);
        perror("Execution failed");
        exit(EXIT_FAILURE);
            

        
    } else { // Parent process

        //printf("background: %d",background);
        if (!background) {
            int status;
            waitpid(pid, &status, 0);
        }/*else{
            process_count++;
            //printf("of a background process");
        }*/        
        
    }
    free(success_executable_path);
}

// Open the file if it exists, otherwise create it
FILE* openOrCreateFile(const char* filename, const char* mode) {
    FILE* file = fopen(filename, mode);
    if (file == NULL) {
        // File does not exist, create it
        file = fopen(filename, "w");
        if (file != NULL) {
            printf("File '%s' created.\n", filename);
        } else {
            fprintf(stderr, "Error creating file '%s'.\n", filename);
        }
    }
    return file;
}

// Add an alias to the aliases file
void addAliasToFile(const char* filename, const char* alias_name, const char* command) {
    FILE* alias_file = openOrCreateFile(filename, "a");
    if (alias_file != NULL) {
        fprintf(alias_file, "%s %s\n", alias_name, command);
        fclose(alias_file);
        //printf("Alias '%s' created for '%s'\n", alias_name, command);
    } else {
        fprintf(stderr, "Error opening the aliases file for writing.\n");
    }
}

// Format: alias alias_name = "command_name command_args"
// Example: alias egemen = "gcc main.c -o executable.o"
void handle_alias(char **tokens) {
    char alias_name[128];
    strcpy(alias_name, tokens[1]);

    char command[256] = "";
    for (int i = 3; tokens[i] != NULL; ++i) {
        strcat(command, tokens[i]);
        strcat(command, " ");
    }

    // Remove the trailing and leading quotes
    command[strlen(command) - 2] = '\0';
    truncateFirstIndex(command);

    addAliasToFile(filename, alias_name, command);

    strcpy(aliases[alias_count].alias_name, alias_name);
    strcpy(aliases[alias_count].command, command);
    alias_count++;  
    
    
}

//display the information of the shell
void display_bello_info(char** tokens) {

    // 1. Username
    char *username = getenv("USER");

    // 2. Hostname
    char hostname[128];
    gethostname(hostname, sizeof(hostname));

    // 3. Last Executed Command

    // 4. TTY
    char *tty = ttyname(STDIN_FILENO);

    // 5. Current Shell Name
    //char *shell_name = getenv("SHELL");

    // 6. Home Location
    char *home_location = getenv("HOME");

    // 7. Current Time and Date
    time_t current_time;
    time(&current_time);
    char time_string[64];
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", localtime(&current_time));


    // 8. Current number of processes being executed
    // calculate it with ps -t 
    // run the command in the child process and read the output in the parent process via pipe
    int line_count = 0;

    for (int i = 0; tokens[i] != NULL; ++i) {
        //free(tokens[i]);
        tokens[i] = NULL;
    }
    //write tokens ps -t
    tokens[0] = strdup("ps");
    tokens[1] = strdup("-t");
    tokens[2] = NULL;

    int pipe_fd[2];

    pipe(pipe_fd);
    pid_t pid = fork();

    if (pid == -1) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { 
        
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);
        

        execvp(tokens[0], tokens);
        perror("Execution failed");
        exit(EXIT_FAILURE);
            
    } else { // Parent process

        int status;
        waitpid(pid, &status, 0);

        close(pipe_fd[1]);  // Close the write end in the parent
        //read the output of the command and count the lines
        char buffer[4096];
        ssize_t count = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
        if (count > 0) {
            buffer[count] = '\0';

            for (ssize_t i = 0; i < count; ++i) {
                if (buffer[i] == '\n') {
                    line_count++;
                }
            }
        }
        close(pipe_fd[0]);

    }




    // Display information
    printf("1. Username: %s\n", username);
    printf("2. Hostname: %s\n", hostname);
    printf("3. Last Executed Command: %s\n", last_command); // Uncomment when tracking last command is implemented
    printf("4. TTY: %s\n", tty);
    //printf("5. Current Shell Name: %s\n", shell_name);
    printf("5. Current Shell Name: %s", current_shell_name);
    printf("6. Home Location: %s\n", home_location);
    printf("7. Current Time and Date: %s\n", time_string);
    //printf("8. Current number of processes being executed: %d\n", process_count);
    printf("8. Current number of processes being executed: %d\n", (line_count-5));

}
