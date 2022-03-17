# ECSE 427 Project 3 (Fall 2021)

## Description
In this project, a simple file system (SFS) that can be mounted by the user under a directory in the user’s machine is designed and implemented. 
The SFS is only working only in Linux. 
The SFS introduces many limitations such as restricted filename lengths, no user concept, no protection among files, no support for concurrent access, etc. 
Furthermore, FUSE is also tested to be working properly in this system. The files from the user are stored on the disk and features, e.g. create, read, write and open, are implemented by using **i-Node Table** and **Bitmap**. **GDB** was used for debugging and the system was checked to be **free from memory leak**.

## Project Structure

```console
.
├── Project Description.pdf
├── README.md
└── Codes
    ├── Makefile
    ├── disk_emu.c
    ├── disk_emu.h
    ├── fuse_wrap_new.c
    ├── fuse_wrap_new.h
    ├── sfs_api.c
    ├── sfs_api.h
    ├── sfs_newfile
    ├── sfs_oldfile
    ├── sfs_test1.c
    ├── sfs_test2.c
    └── sfs_test3.c
```
