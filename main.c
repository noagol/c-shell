
#define _POSIX_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>


#define INPUT_SIZE 512
#define SPACE " \t\r\n\a"

#define CHANGEDIR_CMMND "cd"
#define EXIT_CMMND "exit"
#define JOBS_CMMND "jobs"


int stopProgram = 0;
int statusCode = 0;

/**
 * A struct to handle job commands
 */
typedef struct Job Job;
struct Job {
    pid_t pid;
    char command[INPUT_SIZE];
    Job *next;
};

int arrayLength(char *arg[INPUT_SIZE]);

void getLine(char buf[INPUT_SIZE]);

int parser(char *input, char *parsed[INPUT_SIZE]);

Job *deleteJob(Job *head);

Job *getLast(Job *head);

void printJob(Job *head);

void freeJobs(Job *head);

void concatStrings(char *parsed[INPUT_SIZE], char *str);

Job *basicCommand(char *parsed[INPUT_SIZE], int isBackground, Job *head);

void cdCommand(char *parsed[INPUT_SIZE], char lastDirectory[INPUT_SIZE]);

void exitCommand(Job *head, char *arg[INPUT_SIZE]);

Job *doCommand(char *parsed[INPUT_SIZE], int isBackground, Job *head, char lastDirectory[INPUT_SIZE]);


/**
 * Run c shell program
 * @return exit code of the shell program
 */
int main(void) {
    char line[INPUT_SIZE];
    char *parsed[INPUT_SIZE];
    Job *head = NULL;
    char lastDirectory[INPUT_SIZE];
    lastDirectory[0] = '\0';

    // Execute shell commands
    do {
        getLine(line);
        int isBackground = parser(line, parsed);
        if (isBackground == -1) {
            fprintf(stderr, "Error in system call\n");
        } else {
            head = doCommand(parsed, isBackground, head, lastDirectory);
        }
    } while (!stopProgram);

    // Return program exit code
    return statusCode;
}


/**
 * Counts the length of arguments array
 * @param arg arguments array
 * @return length
 */
int arrayLength(char *arg[INPUT_SIZE]) {
    int i = 0;
    while (arg[i] != NULL && i < INPUT_SIZE) {
        i++;
    }
    return i;
}


/**
 * Get input line from user
 * @param buf input buffer
 */
void getLine(char buf[INPUT_SIZE]) {
    printf("> ");
    fgets(buf, INPUT_SIZE, stdin);
    if (buf[0] == '\n') { getLine(buf); }
}

/**
 * Splits input to arguments array
 * @param input line
 * @param parsed splitted args
 * @return is background
 */
int parser(char *input, char *parsed[INPUT_SIZE]) {
    // get input len
    size_t len = strlen(input);
    // put zero in the parsed array
    __bzero(parsed, INPUT_SIZE);
    int i, start = 0;
    int argumentNumber = 0;
    int command = 0;
    int quotation = 0;

    // goes all the chars in the input array
    for (i = 0; i < len; i++) {
        // if we in quotation mark
        if (quotation && input[i] != '"') {
            continue;
        }
        // check which sign we get
        switch (input[i]) {
            case '\n':
            case '\r':
            case '\a':
            case '\t':
            case ' ':
                // if we see command
                if (command) {
                    // put in the end of the command \0
                    input[i] = '\0';
                    // Add arguments
                    parsed[argumentNumber] = input + start;
                    argumentNumber++;
                    command = 0;
                }
                // next command
                start = i + 1;
                break;
                // if we get "
            case '"':
                // if we not in quotation mod
                if (!quotation) {
                    // get into quotation mod
                    quotation = 1;
                    start = i + 1;
                } else {
                    quotation = 0;
                    // put in the end of the command \0
                    input[i] = '\0';
                    // Add arguments
                    parsed[argumentNumber] = input + start;
                    argumentNumber++;
                    command = 0;
                }
                break;
            default:
                command = 1;
        }
    }
    // if we have " open but no " close
    if (quotation) {
        return -1;
    }

    //Check if background job
    int isBackground = (strcmp(parsed[argumentNumber - 1], "&") == 0);
    if (isBackground) {
        parsed[argumentNumber - 1] = NULL;
    }
    parsed[argumentNumber] = NULL;

    return isBackground;

}


/**
 * Delete dead jobs from the linked list
 * @param head of the list
 * @return new head if changed
 */
Job *deleteJob(Job *head) {
    int statusCode;
    Job *current;
    Job *temp;
    pid_t status;

    // Handle the start of the list
    while (head != NULL) {
        // Check status
        status = waitpid(head->pid, &statusCode, WNOHANG);
        if (status < 0) {
            // Jobs is dead
            current = head->next;
            free(head);
            head = current;
        } else {
            break;
        }
    }

    current = head;

    // Handle middle of the list
    while (current != NULL) {
        if (current->next != NULL) {
            status = waitpid(current->next->pid, &statusCode, WNOHANG);
            if (status < 0) {
                temp = current->next->next;
                free(current->next);
                current->next = temp;
            } else {
                current = current->next;
            }
        } else {
            break;
        }
    }

    // Head may have been changed
    return head;
}

/**
 * Get the last job
 * @param head pointer to first job
 * @return last job in the list
 */
Job *getLast(Job *head) {
    // if head is null return null
    if (head == NULL) {
        return NULL;
    }
    // go all the list until last
    while (head->next) {
        head = head->next;
    }
    //return the last
    return head;
}

/**
 * Print all jobs
 * @param head of linked list of jobs
 */
void printJob(Job *head) {
    // goes all jobs
    while (head != NULL) {
        // print
        printf("%d %s\n", head->pid, head->command);
        // go to the next job
        head = head->next;
    }
}

/**
 * Concat a list of strings to a single one
 * @param parsed list of strings
 * @param str output
 */
void concatStrings(char *parsed[INPUT_SIZE], char *str) {
    str[0] = '\0';
    int i = 0;

    // Concatenate
    while (parsed[i] != NULL) {
        strcat(str, parsed[i]);
        strcat(str, " ");
        i++;
    }

    // Line terminator
    str[strlen(str) - 1] = '\0';
}

/**
 * Run a command
 * @param parsed arguments
 * @param isBackground is background
 * @param head jobs list
 * @return new head of jobs list
 */
Job *basicCommand(char *parsed[INPUT_SIZE], int isBackground, Job *head) {
    pid_t pid;
    int statusCode;

    // Fork a child process
    pid = fork();

    if (pid == 0) {
        // Child
        // Execute command
        if (execvp(parsed[0], parsed) < 0) {
            fprintf(stderr, "Error in system call\n");
        }
        exit(0);
    } else {
        // Father
        // Print pid
        printf("%d\n", pid);

        // Check if background
        if (isBackground) {
            // Allocate job
            Job *job = (Job *) malloc(sizeof(Job));
            job->pid = pid;
            concatStrings(parsed, job->command);
            job->next = NULL;

            // Add to jobs list
            if (head == NULL) {
                head = job;
            } else {
                Job *tail = getLast(head);
                if (tail != NULL) {
                    tail->next = job;
                }
            }
        } else {
            // Wait for child process
            waitpid(pid, &statusCode, WUNTRACED);
        }
    }

    return head;

}

/**
 * implement the cd method - the function checks
 * if there is 2 argument exists, and prints an error message if
 * it doesn’t.
 * @param parsed the array of the line
 * @param lastDirectory the last directory
 */
void cdCommand(char *parsed[INPUT_SIZE], char lastDirectory[INPUT_SIZE]) {
    //print pid
    printf("%d\n", getpid());
    // save the array len
    int len = arrayLength(parsed);
    // save current directory
    char currDir[INPUT_SIZE];
    getcwd(currDir, INPUT_SIZE);
    // check if we get cd / cd~
    if (len == 1 || (len == 2 && strcmp(parsed[1], "~") == 0)) {
        // go to home
        chdir(getenv("HOME"));
        strcpy(lastDirectory, currDir);
        // check if we get cd -
    } else if (len == 2 && strcmp(parsed[1], "-") == 0) {
        // check if we have last directory
        if (strlen(lastDirectory) <= 0) {
            // if we dont have print error
            fprintf(stderr, "Error in system call\n");
        } else {
            // go to the last directory
            chdir(lastDirectory);
            strcpy(lastDirectory, currDir);
        }
        // if we get more then two args - print error
    } else if (len > 2 || chdir(parsed[1]) == -1) {
        fprintf(stderr, "Error in system call\n");
        // else go to last directory
    } else {
        strcpy(lastDirectory, currDir);
    }
}

/**
 * Free all allocated memory of jobs
 * @param head of the jobs list
 */
void freeJobs(Job *head) {
    Job *current;
    // go all the jobs
    while (head != NULL) {
        // check if alive
        if (waitpid(head->pid, &statusCode, WNOHANG) == 0) {
            // kill it
            kill(head->pid, SIGKILL);
        }
        // save the next job
        current = head->next;
        // free memory
        free(head);
        // save new head
        head = current;
    }
}

/**
 * Run Exit command
 * @param head jobs list
 * @param arg arguments
 */
void exitCommand(Job *head, char *arg[INPUT_SIZE]) {
    // print pid
    printf("%d\n", getpid());
    int i;
    // len of arg
    int len = arrayLength(arg);

    if (len > 2) {
        fprintf(stderr, "Error in system call\n");
        return;
    } else if (len == 2) {
        // Cast exit code to int
        size_t length = strlen(arg[1]);
        for (i = 0; i < length; i++) {
            if (!isdigit(arg[1][i])) {
                fprintf(stderr, "Error in system call\n");
                return;
            }
        }
        statusCode = atoi(arg[1]);
    } else {
        // Set exit code
        statusCode = 0;
    }

    // Free allocated memory
    freeJobs(head);
    stopProgram = 1;
}

/**
 * Run builtin commands
 * @param parsed arguments given
 * @param isBackground is background boolean
 * @param head jobs list
 * @param lastDirectory last directory
 * @return new head of jobs list
 */
Job *doCommand(char *parsed[INPUT_SIZE], int isBackground, Job *head, char lastDirectory[INPUT_SIZE]) {
    char *command = parsed[0];

    if (strcmp(command, CHANGEDIR_CMMND) == 0) {
        // cd
        cdCommand(parsed, lastDirectory);
    } else if (strcmp(command, EXIT_CMMND) == 0) {
        // exit
        exitCommand(head, parsed);
    } else if (strcmp(command, JOBS_CMMND) == 0) {
        // jobs
        head = deleteJob(head);
        printJob(head);
    } else {
        // Normal command
        head = basicCommand(parsed, isBackground, head);
    }

    return head;
}
