# ECSE 427 Project 2 (Fall 2021)

## Description
In this project a shell interface is implemented that accepts user commands and executes each command in a **separate process**. 
The shell program provides a command prompt, where the user inputs a line of command. 
The shell is responsible for executing the command. The shell program assumes that the first string of the line gives the name of the executable file. 
The remaining strings in the line are considered as arguments for the command. Furthermore, **piping** and **output redirection** are also implemented in this project.

## Attention
- The first constant in sut.c (in line 16), num_of_CEXEC is a parameter that for the user to change.
- When num_of_CEXEC  = 1, one CPU Executor will be created.
- When num_of_CEXEC  = 2, two CPU Executors will be created.
- The mode for the file that needed to be opened is set to READ AND WRITE mode. This means that if the file hasn't be created before running the sut_write(), it won't be run successfully according to the handout. 
- Whenever the sut_write() is called, all the things inside the file opened will be **OVERWRITTEN**!

## Project Structure

```console
.
├── Project Description.pdf
├── README.md
└── Codes
    ├── YAUThreads
    │   ├── YAUThreads.c
    │   ├── YAUThreads.h
    │   └── testYAU.c
    ├── queue
    │   ├── queue.h
    │   └── queue_example.c
    ├── queue.h
    ├── sut.c
    ├── sut.h
    ├── test1.c
    ├── test2.c
    ├── test3.c
    ├── test4.c
    └── test5.c
```
