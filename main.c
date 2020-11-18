//A=287;B=0x35684103;C=malloc;D=73;E=37;F=nocache;G=12;H=random;I=89;J=sum;K=sema
// /usr/bin/gcc -g /home/artem/CLionProjects/os_1/main.c -o /home/artem/CLionProjects/os_1/main -lpthread

#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>
#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <string.h>

#define SIZE 287 * 1024 * 1024
#define WRITE_THREADS_NUMBER 73
#define FILE_SIZE 37 * 1024 * 1024//E
#define NUMBER_OF_FILE 287 / 37 + 1
#define BLOCK_SIZE 12
#define READ_THREADS_NUMBER 89
#define LOG_FILE "/home/artem/os_1/log_memory.txt"
#define FILE_TEMPLATE "/home/artem/os_1/os-%zu"

void create_open_files();

void close_files();

void *fill_segment_and_write_to_file(void *p);

void *calculate_sum_of_file();

void log_memory(char *);

const size_t size_for_one_thread = SIZE / WRITE_THREADS_NUMBER;
struct sem_file {
    sem_t sem;
    int fd;
};
struct sem_file files[NUMBER_OF_FILE];
static volatile bool terminate = false;
static volatile uint32_t count = 0;

int main() {
    remove(LOG_FILE);
    create_open_files();
    printf("Hi, PID: %d\nEnter anything to allocate memory\n", getpid());
    log_memory("Before allocation");
    getchar();

    char *p = (char *) malloc(SIZE);
    puts("Memory successfully allocated");
    log_memory("After allocation");
    getchar();

    char *p_init = p;
    pthread_t write_threads[WRITE_THREADS_NUMBER];
    for (size_t i = 0; i < WRITE_THREADS_NUMBER; i++) {
        pthread_create(&write_threads[i], NULL, fill_segment_and_write_to_file, p);
        p += size_for_one_thread;
    }
    while (count != WRITE_THREADS_NUMBER);
    log_memory("After filling area");
    getchar();

    pthread_t read_threads[READ_THREADS_NUMBER];
    for (size_t i = 0; i < READ_THREADS_NUMBER; i++) {
        pthread_create(&read_threads[i], NULL, calculate_sum_of_file, NULL);
        printf("Created %zu read thread\n", i + 1);
    }
    getchar();

    terminate = true;
    for (size_t i = 0; i < WRITE_THREADS_NUMBER; i++) {
        pthread_join(write_threads[i], NULL);
    }
    for (size_t i = 0; i < READ_THREADS_NUMBER; i++) {
        pthread_join(read_threads[i], NULL);
    }
    free(p_init);
    puts("Memory deallocated");
    log_memory("After deallocation");
    close_files();
    getchar();
    return 0;
}

void create_open_files() {
    for (size_t i = 0; i < NUMBER_OF_FILE; i++) {
        char filename[22];
        sprintf(filename, FILE_TEMPLATE, i + 1);
        files[i].fd = open(filename, O_RDWR | O_CREAT, S_IRWXU);
        ftruncate(files[i].fd, FILE_SIZE);
        sem_init(&files[i].sem, 0, 1);
        printf("Create file " FILE_TEMPLATE "\n", i + 1);
    }
}

void close_files() {
    for (size_t i = 0; i < NUMBER_OF_FILE; i++) {
        close(files[i].fd);
        sem_destroy(&files[i].sem);
    }
}

void *fill_segment_and_write_to_file(void *p) {
    printf("Created write thread for %p\n", p);
    bool k = false;
    while (!terminate) {
        int a = getrandom((char *) p, size_for_one_thread, 0);
        if (a == -1)
            return 0;
        if (!k) {
            k = true;
            count++;
        }
        size_t nf = rand() % NUMBER_OF_FILE - 1;
        sem_wait(&files[nf].sem);
        if (terminate) {
            sem_post(&files[nf].sem);
            return NULL;
        }
        printf("thread %p WRITE to file %zu\n", p, nf + 1);
        for (size_t i = 0; i < size_for_one_thread; i += BLOCK_SIZE) {
            off_t offset = rand() % (FILE_SIZE - BLOCK_SIZE);
            lseek(files[nf].fd, offset, SEEK_SET);
            write(files[nf].fd, p + i, BLOCK_SIZE);
        }
        printf("thread %p RELEASE file %zu\n", p, nf + 1);
        sem_post(&files[nf].sem);
    }
    return NULL;
}

void *calculate_sum_of_file() {
    while (!terminate) {
        size_t nf = rand() % NUMBER_OF_FILE - 1;
        sem_wait(&files[nf].sem);
        if (terminate) {
            sem_post(&files[nf].sem);
            return NULL;
        }
        printf("        READ for SUM file tmp/os-%zu\n", nf + 1);
        uint64_t sum = 0;
        char buff[BLOCK_SIZE];
        const size_t number_of_blocks = FILE_SIZE / BLOCK_SIZE;
        for (size_t i = 0; i < 2 * number_of_blocks; i++) {
            // get random block
            size_t number = (int) (((double) rand() / (RAND_MAX)) * number_of_blocks);
            if (number == 0) number = number_of_blocks;
            lseek(files[nf].fd, (number - 1) * BLOCK_SIZE, SEEK_SET);
            read(files[nf].fd, buff, BLOCK_SIZE);
            for (size_t j = 0; j < BLOCK_SIZE; j++) {
                sum += buff[j];
            }
        }
        sem_post(&files[nf].sem);
        printf("        RELEASE file tmp/os-%zu. SUM = %lu\n", nf + 1, sum);
    }
    return NULL;
}

void write_status(char *str) {
    FILE *f = fopen(LOG_FILE, "a");
    fputs("  ### ", f);
    fputs(str, f);
    fputs(" ###", f);
    fclose(f);
}

void write_to_log(char *str) {
    FILE *f = fopen(LOG_FILE, "a");
    fputs(str, f);
    fclose(f);
}

void log_memory(char *status) {
    write_status(status);
    write_to_log("\n\nfree -h\n");
    system("free -h >> " LOG_FILE);
    char cmd[55];
    sprintf(cmd, "cat /proc/%d/maps >>" LOG_FILE, getpid());
    write_to_log("\nmaps:\n");
    system(cmd);
    write_to_log("\n");
}