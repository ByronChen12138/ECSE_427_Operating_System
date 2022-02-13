# ECSE_427_Operating_System Fall 2021
Operating System coursework in C code at McGill University.
Programming Projects Using C

## **Project 1** - A Simple Shell
In this project a shell interface is implemented that accepts user commands and executes each command in a separate process. The shell program provides a command prompt, where the user inputs a line of command. The shell is responsible for executing the command. The shell program assumes that the first string of the line gives the name of the executable file. The remaining strings in the line are considered as arguments for the command.

## **Project 2** - A Simple User-Level Thread Scheduler
In this project, a simple many-to-many user-level threading library with a simple first come first serve (FCFS) thread scheduler is designed and implemented. The threading library will have two types of executors – the executors are kernel-level threads that run the user-level threads. One type of executor is dedicated to running compute tasks and the other type of executors is dedicated for input-output tasks. Furthermore, this library supports two user-level threads executors work at the same time.

## **Project 3** - A Mountable Simple File System
In this project, a simple file system (SFS) that can be mounted by the user under a directory in the user’s machine is designed and implemented.The SFS is only working only in Linux. The SFS introduces many limitations such as restricted filename lengths, no user concept, no protection among files, no support for concurrent access, etc. Furthermore, FUSE also works properly.
