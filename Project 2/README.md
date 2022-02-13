This file should be run with the given library "queue.h". It is expected to be put in the same folder as the sut.c is. Otherwise, it cannot be compiled successfully.

The first constant (in line 16) , num_of_CEXEC is a parameter that for the user to change.

When num_of_CEXEC  = 1, one CPU Executor will be created which follows the instructions for Part A in handout.

When num_of_CEXEC  = 2, two CPU Executors will be created which follows the instructions for Part B in handout.

The mode for the file that needed to be opened is set to READ AND WRITE mode. This means that if the file hasn't be created before running the sut_write(), it won't be run successfully according to the handout. 
Moreover, whenever the sut_write() is called, all the things inside the file opened will be OVERWRITTEN!!!!!!!!!!
