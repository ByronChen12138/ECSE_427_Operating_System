/*  Assignment1.h
 *
 *  Created on: Oct. 3, 2021
 *      Author: Byron Chen
 */

#ifndef ASSIGNMENT1_H_
#define ASSIGNMENT1_H_

/* Function intHandler
------------------------------------------------------------
Kill the program when receive keyboard signal CTRL+C

sig: the signal intHandler() function receives
*/
void intHandler(int sig);

/* Function tstpHandler
------------------------------------------------------------
Ignore signal CTRL+Z

sig: the signal tstpHandler() function receives
*/
void tstpHandler(int sig);

/* Function: forkProcess
------------------------------------------------------------
(1) fork a child process using fork()
(2) the child process will invoke execvp()
(3) if background is not specified, the parent will wait, 
    otherwise parent starts the next command...

args: The file needed to be run
bg: the background condition

returns: 0 for failure and 1 for success
*/
int forkProcess(char **args, int bg);

/* Function: getcmd
------------------------------------------------------------
Trace the user input and store it

prompt: the prompt used
args[]: the line inputed by the user
background: the background condition

returns: the count of the command
*/
int getcmd(char *prompt, char *args[], int *background);

#endif /* ASSIGNMENT1_H_ */