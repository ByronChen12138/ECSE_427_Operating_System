#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <wait.h>
#include <fcntl.h>

const int PATH_MAX = 1024;
const int COMMAND_MAX = 1024;
char *cmdList[1000];
int jobIdList[1000];
int jobIndex = 0;
int currPid = -1;

/**
 * @brief  Kill the program when receive keyboard signal CTRL+C
 * @note
 * @param  sig: int, the signal intHandler() function receives
 * @retval None
 */
void intHandler(int sig) {
    printf("CTRL + C Received!");

    // If nothing to kill
    if (currPid < 0){
        printf("Don't kill me, kill Bill.\n");
    
    }else{
        kill(currPid, SIGKILL);
    }
}

/**
 * @brief  Ignore signal CTRL+Z
 * @note
 * @param  sig: int, the signal tstpHandler() function receives
 * @retval None
 */
void tstpHandler(int sig) {
    // Do nothing
    printf("CTRL + Z Received!");
}

/**
 * @brief  Do the redirection for the process
 * @note
 * @param  args: the user input
 * @param  signLocation: the location of the filename input
 * @retval None
 */
void doRedirection(char **args, int signLocation) {
    // Get the file name
    char *filename = args[signLocation + 1];

    // Remove the tokens for the redirection
    args[signLocation] = NULL;
    args[signLocation + 1] = NULL;

    // Do Redirection
    printf("The file is: %s\n", filename);
    int fileDescriptor = open(filename, O_WRONLY | O_APPEND);
    printf("The File Descriptor: %d\n", fileDescriptor);
    dup2(fileDescriptor, 1);
    execvp(args[0], args);
}

/**
 * @brief  Do the piping for the process
 * @note
 * @param  args: the user input
 * @param  signLocation: the location of the filename input
 * @retval None
 */
void doPiping(char **args, int signLocation) {
    // Get the cmds
    char **argv = &args[signLocation + 1];
    args[signLocation] = NULL;

    // Create a pipe
    int pf[2];
    pipe(pf);

    // Do fork to create child process for second cmd
    int pid = fork();

    //Fork Error Case
    if (pid < 0) {
        printf("Error: Pipe Fork Failed!\n");
        exit(EXIT_FAILURE);

    // In child process
    } else if (pid == 0) {
        close(pf[0]);
        dup2(pf[1], STDOUT_FILENO);
        execvp(args[0], args);
    
    // In parent process
    } else {
        close(pf[1]);
        dup2(pf[0], STDIN_FILENO);
        execvp(argv[0], argv);
    }
}

/**
 * @brief  Execute the user cmds with give conditions (redirection or piping or both or neither)
 * @note
 * @param  args: the user input
 * @param  cnt: The count of the cmd line
 * @retval None
 */
void userCmdHandler(char **args, int cnt) {
    int isRedirection = 0;
    int isPiping = 0;
    int signLocation = 0;

    // Check the redirection and piping tokens
    for (int i = 0; i < cnt; i++) {
        if (strcmp(args[i], ">") == 0) {
            isRedirection = 1;
            signLocation = i;
        }

        if (strcmp(args[i], "|") == 0) {
            isPiping = 1;
            signLocation = i;
        }
    }

    printf("Is Redirection? (0: NO, 1: YES) %d\n", isRedirection);
    printf("Is Piping? (0: NO, 1: YES) %d\n", isPiping);

    // If this is not redirection or pipe
    if (!isRedirection && !isPiping) {
        execvp(args[0], args);

    // If redirection is needed
    } else if (isRedirection) {
        doRedirection(args, signLocation);

    // If pipe is needed
    } else if (isPiping) {
        doPiping(args, signLocation);

    }
}

/**
 * @brief  (1) fork a child process using fork()
 * * (2) the child process will invoke execvp()
 * * (3) if background is not specified, the parent will wait, otherwise parent starts the next command...
 * @note
 * @param  **args: The file needed to be run
 * @param  bg: the background condition
 * @param  cnt: The count of the cmd line
 * @retval 0 for success and 1 for failure
 */
int forkProcess(char **args, int bg, int cnt) {
    // Fork call
    int pid = fork();

    // Fork error case
    if (pid < 0) {
        printf("Error: Main Fork Failed!\n");
        exit(EXIT_FAILURE);
    }

    // In child process
    else if (pid == 0) {
        // Record the cmd and pid for the jobs dealing with
        userCmdHandler(args, cnt);
        exit(EXIT_SUCCESS);
    
    // In parent process
    } else {
        cmdList[jobIndex] = args[0];
        jobIdList[jobIndex++] = pid;
        currPid = pid;
        
        // Concurrent case
        if (bg == 1) {
            return 0;

        // Not concurrent case
        } else {
            waitpid(pid, NULL, 0);
        }
    }

    return 0;
}

/**
 * @brief  Trace the user input and store it
 * @param  *prompt: the prompt used
 * @param  *args[]: the line input by the user
 * @param  *background: the background condition
 * @retval i: the count of the command
 */
int getcmd(char *prompt, char *args[], int *background) {
    int length, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    // If empty string is entered
    if (length <= 0) {
        exit(-1);
    }

    // Check if background is specified..
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    } else {
        *background = 0;
    }

    while ((token = strsep(&line, " \t\n")) != NULL) {
        for (int j = 0; j < strlen(token); j++) {
            if (token[j] <= 32)
                token[j] = '\0';
        }

        if (strlen(token) > 0) {
            args[i++] = token;

        }
    }

    args[i] = NULL;

    return i;
}

/**
 * @brief  Print the message with new line in the shell
 * @note
 * @param  *s: string needed to be printed out
 * @retval 0
 */
int helpMessage(char *s) {
    printf("%s\n", s);
    return 0;
}

/**
 * @brief  Prints present working directory
 * @note
 * @param  **args: the user input
 * @retval 0 for success and 1 for failure
 */
int pwd(char **args, int cnt) {
    char buf[PATH_MAX];

    if (cnt == 1) {
        size_t size = pathconf(".", _PC_PATH_MAX);
        getcwd(buf, size);
        printf("The current directory is: %s\n", buf);
        return 0;
    }

    return helpMessage("Error: The 'pwd' Command Has 0 Argument!");

}

/**
 * @brief  Change into the given directory, if no args, show current directory
 * @note
 * @param  **args: the user input
 * @param  cnt: The count of the cmd line
 * @retval 0 for success and 1 for failure
 */
int cd(char **args, int cnt) {
    if (cnt > 1) {
        return chdir(args[1]);
    } else {
        return pwd(args, cnt);
    }
}

/**
 * @brief  Terminates the shell
 * @note
 * @param  **args: the user input
 * @param  cnt: The count of the cmd line
 * @retval 0 for success and 1 for failure
 */
int myExit(char **args, int cnt) {
    if (cnt == 1) {
        puts("Shell closed!");
        exit(0);
    }

    return helpMessage("Error: The 'exit' Command Has 0 Argument!");
}

/**
 * @brief  Brings the background jobs to the foreground, can do specified job
 * @note
 * @param  **args: the user input
 * @param  cnt: The count of the cmd line
 * @retval 0 for success and 1 for failure
 */
int fg(char **args, int cnt) {
    int job = 0;

    // Get the job num
    if (cnt == 1) {
        job = atoi(args[1]);
    }

    waitpid(jobIdList[job], NULL, 0);

    return 0;
}

/**
 * @brief  Change into the given directory
 * @note
 * @param  **args: the user input
 * @param  cnt: The count of the cmd line
 * @retval 0 for success and 1 for failure
 */
int jobs(char **args, int cnt) {
    if (cnt == 1) {
        for (int j = 0; j <= jobIndex; j++) {         // Check all the processes stored
            if (waitpid(jobIdList[j], NULL, WNOHANG) == 0) {    // If the checking process is running
                printf("ID: %d, cmd: %s, PID: %d\n", j, cmdList[j], jobIdList[j]);
            }
        }

        return 0;
    }

    return helpMessage("Error: The 'jobs' Command Has 0 Argument!");
}

/**
 * @brief  Check if the input cmd is a built-in function
 * @note
 * @param  **args: the user input
 * @retval 0 if true and 1 if false
 */
int isBuiltIn(char **args) {
    char *cmd = args[0];
    if ((strcmp(cmd, "cd") == 0) ||
        (strcmp(cmd, "pwd") == 0) ||
        (strcmp(cmd, "exit") == 0) ||
        (strcmp(cmd, "fg") == 0) ||
        (strcmp(cmd, "jobs") == 0)) {
        return 0;
    }

    return 1;
}

/**
 * @brief  Do corresponding action according to the cmd received: cd | pwd | exit | fg | jobs
 * @note
 * @param  **args: The cmd needed to be run
 * @param  cnt: The count of the cmd line
 * @retval 0 for success and 1 for failure
 */
int builtInHandler(char **args, int cnt) {
    // If empty string is entered
    if (cnt <= 0) return 1;

    // Duplicate the first string
    char *cmd = strdup(args[0]);

    // Check the cmd with each function
    if (strcmp(cmd, "cd") == 0) {
        return cd(args, cnt);

    } else if (strcmp(cmd, "pwd") == 0) {
        return pwd(args, cnt);

    } else if (strcmp(cmd, "exit") == 0) {
        return myExit(args, cnt);

    } else if (strcmp(cmd, "fg") == 0) {
        return fg(args, cnt);

    } else if (strcmp(cmd, "jobs") == 0) {
        return jobs(args, cnt);

    } else {
        helpMessage("Error: Could Not Find The Command!\n");
        return 1;
    }
}

int main(void) {
    char *args[COMMAND_MAX];
    int bg;

    // Check the signals
    if (signal(SIGINT, intHandler) == SIG_ERR) {                // If intHandler() fails
        printf("Error: Could Not Bind The SIGINT Handler!\n");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGTSTP, tstpHandler) == SIG_ERR) {              // If tstpHandler() fails
        printf("Error: Could Not Bind The SIGTSTP Handler!\n");
        exit(EXIT_FAILURE);
    }

    // Check the cmd line
    while (1) {
        bg = 0;
        int cnt = getcmd("\n>> ", args, &bg);

        if (cnt == 0) continue;

        if (isBuiltIn(args) == 0)      // If the command is a built-in function
            builtInHandler(args, cnt);       // Execute it in the main thread
        else                          // If the cmd is a user function
            forkProcess(args, bg, cnt);      // Do fork
    }
}