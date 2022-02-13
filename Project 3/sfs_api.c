#include "sfs_api.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "disk_emu.h"

//-------------------- Constants --------------------

#define MAX_FILE_NAME_LENGTH 30
#define MAX_FILE_EXTENSION_LENGTH 3
#define BLOCK_SIZE 1024
#define NUM_OF_BLOCKS 1024
#define NUM_OF_I_NODES 150
#define NUM_OF_FILES (NUM_OF_I_NODES - 1)
#define DIRECT_PTR_NUM 12
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
//-------------------- Structures --------------------

typedef struct super_block {
    int magic_number;
    int block_size;          // The size of each block
    int file_system_size;    // The total number of blocks in the system
    int i_node_table_length; // The number of blocks contain i-nodes
    int root_directory;      // The pointer to the i-node for the root directory
} super_block;

typedef struct fd {
    int i_node_ptr;
    int read_write_ptr;
    bool occupied;
} fd;

typedef struct i_node_entry {
    int size;
    int direct_ptr[DIRECT_PTR_NUM];
    int indirect_ptr;
    bool occupied;
    int mode, link_cnt, uid, gid;
} i_node_entry;

typedef struct root_entry {
    char file_name[MAX_FILE_NAME_LENGTH + 1 + MAX_FILE_EXTENSION_LENGTH];
    int i_node_ptr;
    bool occupied;
} root_entry;

//------------------ Global Variables ------------------

fd fdt[NUM_OF_FILES];
i_node_entry i_node_table[NUM_OF_I_NODES];
root_entry root_dir_table[NUM_OF_FILES];
unsigned char free_bitmap[NUM_OF_BLOCKS];

// Variables for storing them onto the disk
int i_node_table_start_point = 1;
int i_node_table_end_point = 12;
int root_directory_table_start_point = 13;
int root_directory_table_end_point = 17;
int data_blocks_start_point = 18;
int free_bitmap_start_point = 1023;

int curr_file_index = 0;
int num_of_files_visited = 0;
//------------------ Helper Functions ------------------
void initialize_i_node_table();
void initialize_root_directory_table();
void initialize_free_bitmap();
void set_bitmap_free(int location);
void set_bitmap_not_free(int location);
int allocate_disk_block_to_i_node(i_node_entry *i_node);
void save_i_node_table();
void save_free_bitmap();
void save_root_directory_table();

//------------------  Initializations ------------------

/**
 * @brief  Initialize the super block
 * @note
 * @retval None
 */
void initialize_super_block() {
    struct super_block super_block;
    super_block.block_size = BLOCK_SIZE;
    super_block.file_system_size = NUM_OF_BLOCKS;
    super_block.i_node_table_length = i_node_table_end_point - i_node_table_start_point + 1;
    super_block.magic_number = 0x888;
    super_block.root_directory = 0;
    write_blocks(0, 1, &super_block);
}

/**
 * @brief  Initialize the i-node table
 * @note
 * @retval None
 */
void initialize_i_node_table() {
    // Initialize all the i-nodes in the table
    for (int i = 0; i < NUM_OF_I_NODES; i++) {
        i_node_table[i].size = -1;
        i_node_table[i].indirect_ptr = -1;
        for (int j = 0; j < DIRECT_PTR_NUM; j++) {
            i_node_table[i].direct_ptr[j] = -1;
        }
        i_node_table[i].occupied = false;
    }

    // Initialize root i-node
    i_node_entry *root_i_node = &i_node_table[0];
    int root_directory_length = i_node_table_start_point - i_node_table_end_point + 1;
    root_i_node->size = NUM_OF_FILES * sizeof(root_entry);
    for (int i = 0; i < root_directory_length; i++) {
        root_i_node->direct_ptr[i] = i + root_directory_table_start_point;
    }
    root_i_node->occupied = true;

    save_i_node_table();
}

/**
 * @brief  Initialize the root directory
 * @note
 * @retval None
 */
void initialize_root_directory_table() {
    for (int i = 0; i < NUM_OF_FILES - 1; i++) {
        root_dir_table[i].i_node_ptr = -1;
        root_dir_table[i].occupied = false;
    }
    save_root_directory_table();
}

/**
 * @brief  Initialize the free bitmap
 * @note
 * @retval None
 */
void initialize_free_bitmap() {
    memset(free_bitmap, 0, NUM_OF_BLOCKS);
    for (int i = data_blocks_start_point; i < free_bitmap_start_point; i++)
        set_bitmap_free(i);
}

/**
 * @brief Initialize the FDT
 * @note
 * @retval None
 */
void initialize_FDT() {
    for (int i = 0; i < NUM_OF_FILES; i++) {
        fdt[i].i_node_ptr = -1;
        fdt[i].read_write_ptr = -1;
        fdt[i].occupied = false;
    }
}

//-------------------- Bitmap Utils --------------------
/**
 * @brief Find the first free block by the free bitmap and occupy it
 * @note
 * @retval return the location of the allocated block; -1 if unsuccessful.
 */
int allocate_a_block() {
    int loc = -1;
    for (int i = 0; i < NUM_OF_BLOCKS; i++)
        if (free_bitmap[i] == 1) {
            loc = i;
            break;
        }

    if (loc == -1)
        return -1;

    set_bitmap_not_free(loc);

    return loc;
}

/**
 * @brief  Set the bit with given location to 1 in the bitmap and save in the disk
 * @note
 * @param  location: The bit to be set
 * @retval None
 */
void set_bitmap_free(int location) {
    free_bitmap[location] = 1;
    save_free_bitmap();
}

/**
 * @brief  Set the bit with given location to 0 in the bitmap and save in the disk
 * @note
 * @param  location: The bit to be set
 * @retval None
 */
void set_bitmap_not_free(int location) {
    free_bitmap[location] = 0;
    save_free_bitmap();
}

//------------------ Root Table Utils ------------------
/**
 * @brief  Find the first free root directory
 * @note
 * @retval Return the location of the first free root directory; -1 if no free block
 */
int get_the_first_free_root_directory() {
    for (int i = 0; i < NUM_OF_FILES; i++)
        if (!root_dir_table[i].occupied)
            return i;

    return -1;
}

/**
 * @brief  Get the file given the filename by grid searching the root directory table
 * @note
 * @param  *filename: The name of the file
 * @retval Return the file, NULL if not found
 */
root_entry *get_file(const char *filename) {
    for (int i = 0; i < NUM_OF_FILES; i++)
        if (strcmp(root_dir_table[i].file_name, filename) == 0)
            return &root_dir_table[i];

    return NULL;
}

/**
 * @brief Get the number of the files by grid searching the root directory table
 * @note
 * @retval The number of the files
 */
int get_num_of_files() {
    int count = 0;
    for (int i = 0; i < NUM_OF_FILES; i++)
        count += root_dir_table[i].occupied ? 1 : 0;
    return count;
}

/**
 * @brief  Check if the given file exist by grid searching the root directory table
 * @note
 * @param  *filename: the name of the given file to be checked
 * @retval true if it is found; otherwise false
 */
bool does_file_exist(const char *filename) {
    for (int i = 0; i < NUM_OF_FILES; i++)
        if (root_dir_table[i].occupied && strcmp(root_dir_table[i].file_name, filename) == 0)
            return true;
    return false;
}

//-------------------- I-node Utils --------------------
/**
 * @brief  Find the first free i-node
 * @note
 * @retval Return the location of the first free i-node; -1 if no free i-node
 */
int get_the_first_free_i_node() {
    for (int i = 0; i < NUM_OF_I_NODES; i++)
        if (!i_node_table[i].occupied)
            return i;

    return -1;
}

/**
 * @brief  Get the end of the given file
 * @note
 * @param  *filename: The name of the file
 * @retval The address of the end of the file
 */
int end_of_file(char *filename) { return i_node_table[get_file(filename)->i_node_ptr].size; }

/**
 * @brief  Allocate a disk block and update the corresponding pointer in i-node
 * @note
 * @param  i_node: The i_node needed to be updated
 * @retval return the location of the allocated disk block, -1 otherwise
 */
int allocate_disk_block_to_i_node(i_node_entry *i_node) {
    int free_block = allocate_a_block();

    // If no free block is left
    if (free_block == -1) {
        return -1;
    }

    bool is_direct_full = true;
    for (int i = 0; i < 12; i++)
        if (i_node->direct_ptr[i] == -1)
            is_direct_full = false;

    if (is_direct_full) {
        // Allocate the block to with the indirect pointer if no free direct pointer
        int indirect_block[BLOCK_SIZE / sizeof(int)];

        if (i_node->indirect_ptr == -1) {
            int temp = allocate_a_block();

            if (temp == -1) {
                // If there is no free block for the indirect block
                return -1;
            }

            i_node->indirect_ptr = temp;
            memset(indirect_block, -1, BLOCK_SIZE);
            write_blocks(temp, 1, indirect_block);
        } else
            read_blocks(i_node->indirect_ptr, 1, indirect_block);

        // Find a vacant spot.
        int idx = 0;
        while (idx < BLOCK_SIZE / sizeof(int) && indirect_block[idx] != -1)
            idx++;
        if (idx == BLOCK_SIZE / sizeof(int))
            return -1;

        indirect_block[idx] = free_block;
        write_blocks(i_node->indirect_ptr, 1, indirect_block);
        save_i_node_table();
        return free_block;

    } else {
        // Allocate the block to with the direct pointer
        for (int i = 0; i < 12; i++) {
            if (i_node->direct_ptr[i] == -1) {
                i_node->direct_ptr[i] = free_block;
                save_i_node_table();
                return free_block;
            }
        }
    }
}
//---------------------- FDT Utils ---------------------
/**
 * @brief  Get the number of the opened files by grid searching the FDT
 * @note
 * @retval The number of the opened files
 */
int get_num_of_opened_files() {
    int count = 0;
    for (int i = 0; i < NUM_OF_FILES; i++)
        if (fdt[i].occupied)
            count++;
    return count;
}

/**
 * @brief  Find the first free block in FDT
 * @note
 * @retval Return the location of the first free block; -1 if no free block
 */
int fdt_get_the_first_free_block() {
    for (int i = 0; i < sizeof(fdt); i++)
        if (!fdt[i].occupied)
            return i;
    return -1;
}

/**
 * @brief  Get the file descriptor give i-node ptr
 * @note
 * @param  i_node_ptr: The ptr to the i-node contains the file
 * @retval file descriptor of the file, -1 if not found
 */
int get_fd(int i_node_ptr) {
    for (int i = 0; i < NUM_OF_FILES; i++)
        if (fdt[i].i_node_ptr == i_node_ptr && fdt[i].occupied)
            return i;
    return -1;
}

//------------------ Flushing Utils ------------------
/**
 * @brief  Save the i-node table onto the disk
 * @note
 * @retval None
 */
void save_i_node_table() {
    int i_node_table_length = i_node_table_end_point - i_node_table_start_point + 1;
    write_blocks(i_node_table_start_point, i_node_table_length, &i_node_table);
}

/**
 * @brief  Save the root directary table onto the disk
 * @note
 * @retval None
 */
void save_root_directory_table() {
    int root_directory_table_length =
        root_directory_table_end_point - root_directory_table_start_point + 1;
    write_blocks(root_directory_table_start_point, root_directory_table_length, &root_dir_table);
}

/**
 * @brief  Save the free bitmap onto the disk
 * @note
 * @retval None
 */
void save_free_bitmap() { write_blocks(free_bitmap_start_point, 1, &free_bitmap); }

//------------------ Main Functions ------------------

/**
 * @brief fresh = 0: the file system is opened from the disk;
 * * fresh = 1: create a new file system with default settings
 * @note
 * @param  fresh: the flag shows if a new file system is needed.
 * @retval None
 */
void mksfs(int fresh) {
    if (fresh) {
        init_fresh_disk("Byron_sfs.txt", BLOCK_SIZE, NUM_OF_BLOCKS);
        initialize_super_block();
        initialize_i_node_table();
        initialize_root_directory_table();
        initialize_free_bitmap();

    } else {
        init_disk("Byron_sfs.txt", BLOCK_SIZE, NUM_OF_BLOCKS);
        // Read the i-node table
        read_blocks(i_node_table_start_point, i_node_table_end_point - i_node_table_start_point + 1,
                    i_node_table);

        // Read root directory
        read_blocks(root_directory_table_start_point,
                    root_directory_table_end_point - root_directory_table_start_point + 1,
                    root_dir_table);

        // Read the the free bitmap
        read_blocks(free_bitmap_start_point, 1, free_bitmap);
    }
    initialize_FDT();
}

/**
 * @brief Get the name of the next file in directory
 * @note
 * @param  *fname: Store the filename here
 * @retval return 1 if next file is found, 0 otherwise
 */
int sfs_getnextfilename(char *fname) {
    int num_of_files = get_num_of_files();

    // If no file is left
    if (num_of_files == 0) {
        return 0;
    } else {
        while (curr_file_index < NUM_OF_FILES &&
               does_file_exist(root_dir_table[curr_file_index].file_name) != 1) {
            curr_file_index++;
        }

        if (curr_file_index == NUM_OF_FILES) {
            curr_file_index = 0;
            return 0;
        }

        char *file_name = root_dir_table[curr_file_index].file_name;
        strcpy(fname, file_name);
        curr_file_index++;

        return 1;
    }
}

/**
 * @brief  Get the size of the given file
 * @note
 * @param  *path: The path of the file
 * @retval return the size of the given file; -1 if file not found
 */
int sfs_getfilesize(const char *path) {
    if (!does_file_exist(path)) {
        printf("The does not exist!");
        return -1;
    } else {
        root_entry *file = get_file(path);
        i_node_entry i_node = i_node_table[file->i_node_ptr];
        return i_node.size;
    }
}

/**
 * @brief  Open the given file, if the file is not created, create it
 * @note
 * @param  *name: The name of the file
 * @retval The file descriptor of the file; -1 if failed
 */
int sfs_fopen(char *name) {
    if (strlen(name) > MAX_FILE_NAME_LENGTH + 1 + MAX_FILE_EXTENSION_LENGTH)
        return -1;

    // If it exists in the FDT, find and return.
    root_entry *r = get_file(name);
    if (r != NULL) {
        int fd = get_fd(r->i_node_ptr);
        if (fd != -1)
            return fd;
    }

    int available_fdt = fdt_get_the_first_free_block();
    if (available_fdt == -1)
        return -1;

    // If the file is already created
    if (does_file_exist(name)) {
        // Open the given file
        root_entry file = *get_file(name);
        fdt[available_fdt].i_node_ptr = file.i_node_ptr;
        fdt[available_fdt].read_write_ptr = end_of_file(name);
        fdt[available_fdt].occupied = true;

    } else {
        // Create the file and store it onto the disk
        int free_root_block = get_the_first_free_root_directory();
        int free_i_node = get_the_first_free_i_node();

        strcpy(root_dir_table[free_root_block].file_name, name);
        root_dir_table[free_root_block].i_node_ptr = free_i_node;
        root_dir_table[free_root_block].occupied = true;

        i_node_table[free_i_node].size = 0;
        i_node_table[free_i_node].occupied = true;
        memset(i_node_table[free_i_node].direct_ptr, -1, DIRECT_PTR_NUM);
        i_node_table->indirect_ptr = -1;

        fdt[available_fdt].i_node_ptr = free_i_node;
        fdt[available_fdt].read_write_ptr = 0;
        fdt[available_fdt].occupied = true;

        // Flush the change to disk
        save_i_node_table();
        save_root_directory_table();
    }

    return available_fdt;
}

/**
 * @brief Close the given file.
 *
 * @param fd The file descriptor for the file to be closed
 * @return 1 if success, otherwise -1
 */
int sfs_fclose(int fd) {
    if (!fdt[fd].occupied)
        return -1;
    fdt[fd].occupied = false;
    fdt[fd].i_node_ptr = -1;
    fdt[fd].read_write_ptr = -1;
    return 0;
}

/**
 * @brief  Recursive function of sfs_fwrite()
 * @note
 * @param  *fd: The file descriptor to be written
 * @param  *i_node: The i_node contains the file
 * @param  *buf: the buf with the information to write
 * @param  length: the length of the information to write
 * @retval number of the written bytes
 */
int sfs_fwrite_recursive(fd *fd, i_node_entry *i_node, char *buf, int length) {
    if (length <= 0)
        return 0;

    int ptr = fd->read_write_ptr;
    int size = i_node->size;

    int current_block = -1;
    bool should_add_new = ptr % BLOCK_SIZE == 0 && ptr >= size;

    if (should_add_new) {
        current_block = allocate_disk_block_to_i_node(i_node);
    } else {
        if (ptr < 12 * BLOCK_SIZE)
            // If the ptr is in the direct blocks
            current_block = i_node->direct_ptr[ptr / BLOCK_SIZE];

        else {
            // If the ptr is in the indirect block
            int indirect_ptr = i_node->indirect_ptr;
            int indirect_block[BLOCK_SIZE / sizeof(int)];
            read_blocks(indirect_ptr, 1, indirect_block);
            int index = ptr - 12 * BLOCK_SIZE;
            current_block = indirect_block[index / BLOCK_SIZE];
        }
    }

    if (current_block == -1)
        return 0;

    char temp[BLOCK_SIZE];
    read_blocks(current_block, 1, temp);
    int bytes_to_write = MIN(BLOCK_SIZE, length);
    bytes_to_write = MIN(bytes_to_write, BLOCK_SIZE - ptr % BLOCK_SIZE);
    memcpy(temp + ptr % BLOCK_SIZE, buf, bytes_to_write);
    write_blocks(current_block, 1, temp);

    fd->read_write_ptr += bytes_to_write;
    i_node->size = MAX(i_node->size, ptr + bytes_to_write);
    return bytes_to_write +
           sfs_fwrite_recursive(fd, i_node, buf + bytes_to_write, length - bytes_to_write);
}

/**
 * @brief  write buf to the file recursively
 * @param  fileID: The id of the file descriptor
 * @param  *buf: the buf with the information to write
 * @param  length: the length of the information to write
 * @retval returns the number of bytes written; -1 if not success
 */
int sfs_fwrite(int fileID, const char *buf, int length) {
    fd *f = &fdt[fileID];
    if (!f->occupied)
        return -1;

    int result = sfs_fwrite_recursive(f, &i_node_table[f->i_node_ptr], buf, length);
    save_root_directory_table();
    save_i_node_table();
    save_free_bitmap();
    return result;
}

/**
 * @brief  Recursive function of sfs_fread()
 * @note
 * @param  *fd: The file descriptor to be written
 * @param  *i_node: The i_node contains the file
 * @param  *buf: the buf used to save the read information
 * @param  length: the length of the information to read
 * @retval number of the read bytes
 */
int sfs_fread_recursive(fd *fd, i_node_entry *node, char *buf, int length) {
    if (length <= 0 || fd->read_write_ptr == node->size)
        return 0;

    int ptr = fd->read_write_ptr;
    int size = node->size;

    int current_block;

    if (ptr >= size)
        current_block = -1;
    else if (ptr < 12 * BLOCK_SIZE)
        // If the ptr is in the direct blocks
        current_block = node->direct_ptr[ptr / BLOCK_SIZE];
    else {
        // If the ptr is in the indirect block
        int indirect_block[BLOCK_SIZE / sizeof(int)];
        read_blocks(node->indirect_ptr, 1, indirect_block);
        int index = ptr - 12 * BLOCK_SIZE;
        current_block = indirect_block[index / BLOCK_SIZE];
    }

    if (current_block == -1)
        return 0;

    int bytes_to_read = MIN(BLOCK_SIZE, length);
    bytes_to_read = MIN(bytes_to_read, BLOCK_SIZE - ptr % BLOCK_SIZE);
    bytes_to_read = MIN(bytes_to_read, size - ptr);

    char block[BLOCK_SIZE];
    read_blocks(current_block, 1, block);
    memcpy(buf, block + ptr % BLOCK_SIZE, bytes_to_read);

    fd->read_write_ptr += bytes_to_read;
    return bytes_to_read +
           sfs_fread_recursive(fd, node, buf + bytes_to_read, length - bytes_to_read);
}

/**
 * @brief  read the file and save to buf recursively
 * @param  fileID: The id of the file descriptor
 * @param  *buf: the buf used to save the read information
 * @param  length: the length of the information to read
 * @retval returns the number of bytes read; -1 if not success
 */
int sfs_fread(int fileID, char *buf, int length) {
    fd *f = &fdt[fileID];
    if (!f->occupied)
        return -1;

    int result = sfs_fread_recursive(f, &i_node_table[f->i_node_ptr], buf, length);
    save_i_node_table();
    save_free_bitmap();
    save_root_directory_table();
    return result;
}

/**
 * @brief  Seek to the location from beginning
 * @note
 * @param  fileID: The file descriptor
 * @param  loc: The location to seek to
 * @retval return 0 if success, -1 otherwise
 */
int sfs_fseek(int fileID, int loc) {
    fd file = fdt[fileID];

    if (!file.occupied) {
        printf("The file is not opened!");
        return -1;
    }

    fdt[fileID].read_write_ptr = loc;
    return 0;
}

/**
 * @brief  Removes a file from the file system
 * @note
 * @param  *file: The name of the file to be removed
 * @retval returns 1 if success, -1 otherwise
 */
int sfs_remove(char *file) {
    // If the file to be removed does not exist
    if (!does_file_exist(file)) {
        return -1;
    }

    root_entry *re = get_file(file);
    re->occupied = false;
    int inode_id = re->i_node_ptr;
    re->i_node_ptr = -1;
    memset(re->file_name, '\0', MAX_FILE_NAME_LENGTH + 1 + MAX_FILE_EXTENSION_LENGTH);
    i_node_entry *i_node = &i_node_table[inode_id];
    fd *fd = &fdt[get_fd(re->i_node_ptr)];

    // Remove the file descriptor in FDT
    fd->i_node_ptr = -1;
    fd->read_write_ptr = -1;
    fd->occupied = false;

    // Remove the i-node in i-node table
    i_node->size = -1;
    i_node->occupied = false;
    int *dp = i_node->direct_ptr;
    for (int i = 0; i < DIRECT_PTR_NUM; i++) {
        set_bitmap_free(dp[i]);
        dp[i] = -1;
    }
    int ind = i_node->indirect_ptr;
    i_node->indirect_ptr = -1;

    if (ind != -1) {
        // Get all the blocks stored on the disk pointed by the indirect_block pointer
        int indirect_block[BLOCK_SIZE / sizeof(int)];
        read_blocks(ind, 1, indirect_block);

        // Remove the things stored in blocks pointed by the indirect_block pointers
        for (int i = 0; i < BLOCK_SIZE / sizeof(int) && indirect_block[i] != -1; i++)
            set_bitmap_free(indirect_block[i]);
    }

    // Store all the upd in on disk
    save_i_node_table();
    save_free_bitmap();
    save_root_directory_table();
    return 1;
}