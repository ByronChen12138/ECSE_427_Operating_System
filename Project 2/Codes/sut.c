#include "sut.h"
#include "queue.h"

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>

// !!!!!! The files are set to READ AND WRITE MODE !!!!!!

// !!!!!! 1 for Part A and 2 for Part B !!!!!!
const int num_of_CEXEC = 1;

/**
 * @brief  A built-in type which is used to record the context to the related thread id
 * @note
 * @retval None
 */
typedef struct threaddesc {
    pid_t thread_id;
    ucontext_t *parent_thread;
} threaddesc;

const int THREAD_STACK_SIZE = 1024 * 64;

int num_of_thread;
int num_of_user_threads;
bool is_running;

struct queue ready_queue; // To store the contexts created, for CPU
struct queue wait_queue;  // To store the contexts created, for I/O
struct queue entry_created_queue;
struct threaddesc *thread_array[3]; // The array to record all the existing thread description

pthread_mutex_t num_of_thread_lock;
pthread_mutex_t ready_queue_lock;
pthread_mutex_t wait_queue_lock;
pthread_mutex_t thread_array_lock;
pthread_mutex_t num_of_user_thread_lock;
pthread_mutex_t entry_created_queue_lock;

pthread_t *CEXEC;
pthread_t *CEXEC_1;
pthread_t *CEXEC_2;
pthread_t *IEXEC;

// ------------------ Helper Methods ------------------
pid_t get_thread_id(void) { return syscall(SYS_gettid); }

/**
 * @brief  Get the CEXEC thread it was runing in
 * @note
 * @param  thread_id: The thread id given for seaching for its parent CEXEC
 * @retval
 */
ucontext_t *get_parent_thread(pid_t thread_id) {

    pthread_mutex_lock(&thread_array_lock);

    for (int i = 0; i < (num_of_CEXEC + 1); i++) {
        if (thread_array[i]->thread_id == thread_id) {
            pthread_mutex_unlock(&thread_array_lock);

            return thread_array[i]->parent_thread;
        }
    }

    pthread_mutex_unlock(&thread_array_lock);
    return NULL;
}

/**
 * @brief  Kill all the created threads
 * @note
 * @retval None
 */
void kill_all_threads() {
    is_running = false;
    pthread_join(*IEXEC, NULL);
    free(IEXEC);

    if (num_of_CEXEC == 1) {
        pthread_join(*CEXEC, NULL);
        free(CEXEC);
    } else {
        pthread_join(*CEXEC_1, NULL);
        free(CEXEC_1);
        pthread_join(*CEXEC_2, NULL);
        free(CEXEC_2);
    }
}

/**
 * @brief  Free the context given
 * @note
 * @param  *context: The context needed to be free with memory
 * @retval None
 */
void free_context(ucontext_t *context) {
    free(context->uc_stack.ss_sp);
    free(context);
}

// ------------------ Main Functions ------------------

void *C_EXEC() {
    ucontext_t *current_thread = (ucontext_t *)malloc(sizeof(ucontext_t));

    current_thread->uc_stack.ss_sp = (char *)malloc(THREAD_STACK_SIZE);
    current_thread->uc_stack.ss_size = THREAD_STACK_SIZE;
    current_thread->uc_link = 0;
    current_thread->uc_stack.ss_flags = 0;

    getcontext(current_thread);

    threaddesc *current_thread_description = (threaddesc *)malloc(sizeof(threaddesc));

    current_thread_description->thread_id = get_thread_id();
    current_thread_description->parent_thread = current_thread;

    pthread_mutex_lock(&num_of_thread_lock);
    pthread_mutex_lock(&thread_array_lock);
    thread_array[num_of_thread] = current_thread_description;
    num_of_thread++;
    pthread_mutex_unlock(&thread_array_lock);
    pthread_mutex_unlock(&num_of_thread_lock);

    while (true) {
        if (!is_running && !num_of_user_threads) {
            pthread_exit(NULL);
        }

        // Get the next queue_entry to be run
        pthread_mutex_lock(&ready_queue_lock);
        struct queue_entry *next_task = queue_pop_head(&ready_queue);
        pthread_mutex_unlock(&ready_queue_lock);

        if (next_task != NULL) {
            ucontext_t *next_context = (ucontext_t *)next_task->data;
            next_context->uc_link = current_thread;
            swapcontext(current_thread, next_context);

            // Store the queue entrys created for memory free later
            pthread_mutex_lock(&entry_created_queue_lock);
            queue_insert_tail(&entry_created_queue, next_task);
            pthread_mutex_unlock(&entry_created_queue_lock);

        } else {
            usleep(100); // While the ready_queue is empty, sleep for a while to save CPU
        }
    }
}

void *I_EXEC() {
    ucontext_t *current_thread = (ucontext_t *)malloc(sizeof(ucontext_t));

    current_thread->uc_stack.ss_sp = (char *)malloc(THREAD_STACK_SIZE);
    current_thread->uc_stack.ss_size = THREAD_STACK_SIZE;
    current_thread->uc_link = 0;
    current_thread->uc_stack.ss_flags = 0;

    getcontext(current_thread);

    threaddesc *current_thread_description = (threaddesc *)malloc(sizeof(threaddesc));

    current_thread_description->thread_id = get_thread_id();
    current_thread_description->parent_thread = current_thread;

    pthread_mutex_lock(&num_of_thread_lock);
    pthread_mutex_lock(&thread_array_lock);
    thread_array[num_of_thread] = current_thread_description;
    num_of_thread++;
    pthread_mutex_unlock(&thread_array_lock);
    pthread_mutex_unlock(&num_of_thread_lock);

    while (true) {
        if (!is_running && !num_of_user_threads) {
            pthread_exit(NULL);
        }

        // Get the next queue_entry to be run
        pthread_mutex_lock(&wait_queue_lock);
        struct queue_entry *next_task = queue_pop_head(&wait_queue);
        pthread_mutex_unlock(&wait_queue_lock);

        if (next_task != NULL) {
            ucontext_t *next_context = (ucontext_t *)next_task->data;
            next_context->uc_link = current_thread;
            swapcontext(current_thread, next_context);

            // Store the queue entrys created for memory free later
            pthread_mutex_lock(&entry_created_queue_lock);
            queue_insert_tail(&entry_created_queue, next_task);
            pthread_mutex_unlock(&entry_created_queue_lock);

        } else {
            usleep(100); // While the wait_queue is empty, sleep for a while to save CPU
        }
    }
}

/**
 * @brief  Initialize the queues, CEXEC and IEXEC
 * @note
 * @retval None
 */
void sut_init() {
    num_of_thread = 0;
    num_of_user_threads = 0;
    is_running = true;
    ready_queue = queue_create();
    wait_queue = queue_create();

    // Initialize the queues
    queue_init(&ready_queue);
    queue_init(&wait_queue);
    queue_init(&entry_created_queue);

    // Initilize the mutex locks
    pthread_mutex_init(&num_of_thread_lock, NULL);
    pthread_mutex_init(&ready_queue_lock, NULL);
    pthread_mutex_init(&wait_queue_lock, NULL);
    pthread_mutex_init(&thread_array_lock, NULL);
    pthread_mutex_init(&num_of_user_thread_lock, NULL);
    pthread_mutex_init(&entry_created_queue_lock, NULL);

    if (num_of_CEXEC == 1) {
        CEXEC = (pthread_t *)malloc(sizeof(pthread_t));
        IEXEC = (pthread_t *)malloc(sizeof(pthread_t));

        pthread_create(CEXEC, NULL, C_EXEC, NULL);
        pthread_create(IEXEC, NULL, I_EXEC, NULL);
    } else {
        CEXEC_1 = (pthread_t *)malloc(sizeof(pthread_t));
        CEXEC_2 = (pthread_t *)malloc(sizeof(pthread_t));
        IEXEC = (pthread_t *)malloc(sizeof(pthread_t));

        pthread_create(CEXEC_1, NULL, C_EXEC, NULL);
        pthread_create(CEXEC_2, NULL, C_EXEC, NULL);
        pthread_create(IEXEC, NULL, I_EXEC, NULL);
    }
}

/**
 * @brief  add the task into the ready_queue
 * @note
 * @param  fn: The task needed to be excuted
 * @retval
 */
bool sut_create(sut_task_f fn) {
    pthread_mutex_lock(&num_of_user_thread_lock);
    num_of_user_threads++;
    pthread_mutex_unlock(&num_of_user_thread_lock);

    // Create the context for coming task
    ucontext_t *new_context = (ucontext_t *)malloc(sizeof(ucontext_t));

    new_context->uc_stack.ss_sp = (char *)malloc(THREAD_STACK_SIZE);
    new_context->uc_stack.ss_size = THREAD_STACK_SIZE;
    new_context->uc_link = 0;
    new_context->uc_stack.ss_flags = 0;

    getcontext(new_context);
    makecontext(new_context, fn, 0);

    // store the current context into the ready queue
    struct queue_entry *new_task = queue_new_node(new_context);

    pthread_mutex_lock(&ready_queue_lock);
    queue_insert_tail(&ready_queue, new_task);
    pthread_mutex_unlock(&ready_queue_lock);

    return true;
}

/**
 * @brief  stop the task and move the task to the tail of the ready_queue
 * @note
 * @retval None
 */
void sut_yield() {
    ucontext_t *current_context = (ucontext_t *)malloc(sizeof(ucontext_t));

    current_context->uc_stack.ss_sp = (char *)malloc(THREAD_STACK_SIZE);
    current_context->uc_stack.ss_size = THREAD_STACK_SIZE;
    current_context->uc_link = 0;
    current_context->uc_stack.ss_flags = 0;

    getcontext(current_context);

    ucontext_t *parent_context = get_parent_thread(get_thread_id());

    struct queue_entry *new_task = queue_new_node(current_context);

    pthread_mutex_lock(&ready_queue_lock);
    queue_insert_tail(&ready_queue, new_task);
    pthread_mutex_unlock(&ready_queue_lock);

    swapcontext(current_context, parent_context);
}

/**
 * @brief  Terminate the thread
 * @note
 * @retval None
 */
void sut_exit() {
    pthread_mutex_lock(&num_of_user_thread_lock);
    num_of_user_threads--;
    pthread_mutex_unlock(&num_of_user_thread_lock);

    setcontext(get_parent_thread(get_thread_id()));
}

/**
 * @brief  Open the given file
 * @note
 * @param  *dest: the destination directory
 * @retval int fd: The file descriptor
 */
int sut_open(char *dest) {
    int fd = -1;

    ucontext_t *current_context = (ucontext_t *)malloc(sizeof(ucontext_t));

    current_context->uc_stack.ss_sp = (char *)malloc(THREAD_STACK_SIZE);
    current_context->uc_stack.ss_size = THREAD_STACK_SIZE;
    current_context->uc_link = 0;
    current_context->uc_stack.ss_flags = 0;

    getcontext(current_context);

    ucontext_t *parent_context = get_parent_thread(get_thread_id());

    // store the current context into the wait queue
    struct queue_entry *temp_entry = queue_new_node(current_context);

    pthread_mutex_lock(&wait_queue_lock);
    queue_insert_tail(&wait_queue, temp_entry);
    pthread_mutex_unlock(&wait_queue_lock);

    // Give the control to the parent C_EXEC, and leave the rest to I_EXEC
    swapcontext(current_context, parent_context);

    // I_EXEC runs this
    fd = open(dest, O_RDWR, 0777);

    // Put it back to ready_queue for C_EXEC and give the control to I_EXEC
    current_context = (ucontext_t *)malloc(sizeof(ucontext_t));

    current_context->uc_stack.ss_sp = (char *)malloc(THREAD_STACK_SIZE);
    current_context->uc_stack.ss_size = THREAD_STACK_SIZE;
    current_context->uc_link = 0;
    current_context->uc_stack.ss_flags = 0;

    parent_context = get_parent_thread(get_thread_id());

    temp_entry = queue_new_node(current_context);

    pthread_mutex_lock(&ready_queue_lock);
    queue_insert_tail(&ready_queue, temp_entry);
    pthread_mutex_unlock(&ready_queue_lock);
    swapcontext(current_context, parent_context);

    return fd;
}

/**
 * @brief  Write to a file
 * @note
 * @param  fd: The file description
 * @param  *buf: The buffer for the contents to be saved
 * @param  size: The size of the contents
 * @retval None
 */
void sut_write(int fd, char *buf, int size) {
    ucontext_t *current_context = (ucontext_t *)malloc(sizeof(ucontext_t));

    current_context->uc_stack.ss_sp = (char *)malloc(THREAD_STACK_SIZE);
    current_context->uc_stack.ss_size = THREAD_STACK_SIZE;
    current_context->uc_link = 0;
    current_context->uc_stack.ss_flags = 0;

    getcontext(current_context);

    ucontext_t *parent_context = get_parent_thread(get_thread_id());

    // store the current context into the wait queue
    struct queue_entry *temp_entry = queue_new_node(current_context);

    pthread_mutex_lock(&wait_queue_lock);
    queue_insert_tail(&wait_queue, temp_entry);
    pthread_mutex_unlock(&wait_queue_lock);

    // Give the control to the parent C_EXEC, and leave the rest to I_EXEC
    swapcontext(current_context, parent_context);

    // I_EXEC runs this
    write(fd, buf, size);

    // Put it back to ready_queue for C_EXEC and give the control to I_EXEC
    current_context = (ucontext_t *)malloc(sizeof(ucontext_t));

    current_context->uc_stack.ss_sp = (char *)malloc(THREAD_STACK_SIZE);
    current_context->uc_stack.ss_size = THREAD_STACK_SIZE;
    current_context->uc_link = 0;
    current_context->uc_stack.ss_flags = 0;

    parent_context = get_parent_thread(get_thread_id());

    temp_entry = queue_new_node(current_context);

    pthread_mutex_lock(&ready_queue_lock);
    queue_insert_tail(&ready_queue, temp_entry);
    pthread_mutex_unlock(&ready_queue_lock);
    swapcontext(current_context, parent_context);
}

/**
 * @brief  Close the given file
 * @note
 * @param  fd: The file description
 * @retval None
 */
void sut_close(int fd) {
    ucontext_t *current_context = (ucontext_t *)malloc(sizeof(ucontext_t));

    current_context->uc_stack.ss_sp = (char *)malloc(THREAD_STACK_SIZE);
    current_context->uc_stack.ss_size = THREAD_STACK_SIZE;
    current_context->uc_link = 0;
    current_context->uc_stack.ss_flags = 0;

    getcontext(current_context);

    ucontext_t *parent_context = get_parent_thread(get_thread_id());

    // store the current context into the wait queue
    struct queue_entry *temp_entry = queue_new_node(current_context);

    pthread_mutex_lock(&wait_queue_lock);
    queue_insert_tail(&wait_queue, temp_entry);
    pthread_mutex_unlock(&wait_queue_lock);

    // Give the control to the parent C_EXEC, and leave the rest to I_EXEC
    swapcontext(current_context, parent_context);

    // I_EXEC runs this
    close(fd);

    // Put it back to ready_queue for C_EXEC and give the control to I_EXEC
    current_context = (ucontext_t *)malloc(sizeof(ucontext_t));

    current_context->uc_stack.ss_sp = (char *)malloc(THREAD_STACK_SIZE);
    current_context->uc_stack.ss_size = THREAD_STACK_SIZE;
    current_context->uc_link = 0;
    current_context->uc_stack.ss_flags = 0;

    parent_context = get_parent_thread(get_thread_id());

    temp_entry = queue_new_node(current_context);
    pthread_mutex_lock(&ready_queue_lock);
    queue_insert_tail(&ready_queue, temp_entry);
    pthread_mutex_unlock(&ready_queue_lock);
    swapcontext(current_context, parent_context);
}

/**
 * @brief  Read the given file
 * @note
 * @param  fd: The file description
 * @param  *buf: The buffer for the contents to be saved
 * @param  size: The size of the contents
 * @retval
 */
char *sut_read(int fd, char *buf, int size) {
    char *result = NULL;
    int out;

    ucontext_t *current_context = (ucontext_t *)malloc(sizeof(ucontext_t));

    current_context->uc_stack.ss_sp = (char *)malloc(THREAD_STACK_SIZE);
    current_context->uc_stack.ss_size = THREAD_STACK_SIZE;
    current_context->uc_link = 0;
    current_context->uc_stack.ss_flags = 0;

    getcontext(current_context);

    ucontext_t *parent_context = get_parent_thread(get_thread_id());

    // store the current context into the wait queue
    struct queue_entry *temp_entry = queue_new_node(current_context);

    pthread_mutex_lock(&wait_queue_lock);
    queue_insert_tail(&wait_queue, temp_entry);
    pthread_mutex_unlock(&wait_queue_lock);

    // Give the control to the parent C_EXEC, and leave the rest to I_EXEC
    swapcontext(current_context, parent_context);

    // I_EXEC runs this
    out = read(fd, buf, size);

    // Put it back to ready_queue for C_EXEC and give the control to I_EXEC
    current_context = (ucontext_t *)malloc(sizeof(ucontext_t));

    current_context->uc_stack.ss_sp = (char *)malloc(THREAD_STACK_SIZE);
    current_context->uc_stack.ss_size = THREAD_STACK_SIZE;
    current_context->uc_link = 0;
    current_context->uc_stack.ss_flags = 0;

    parent_context = get_parent_thread(get_thread_id());

    temp_entry = queue_new_node(current_context);
    pthread_mutex_lock(&ready_queue_lock);
    queue_insert_tail(&ready_queue, temp_entry);
    pthread_mutex_unlock(&ready_queue_lock);

    if (out > 1) {
        result = "Read Successfully!";
    }

    swapcontext(current_context, parent_context);

    return result;
}

/**
 * @brief  Shut down all the threads
 * @note
 * @retval None
 */
void sut_shutdown() {
    kill_all_threads();

    // Clear memory
    for (int i = 0; i < (num_of_CEXEC + 1); i++) {
        free_context(thread_array[i]->parent_thread);
        free(thread_array[i]);
    }

    struct queue_entry *entry_to_delete = queue_pop_head(&entry_created_queue);
    while (entry_to_delete != NULL) {
        free_context(entry_to_delete->data);
        free(entry_to_delete);
        entry_to_delete = queue_pop_head(&entry_created_queue);
    }

    puts("SUT closed!");
}