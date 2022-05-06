// #define _GNU_SOURCE             /* See feature_test_macros(7) */
// #include <sched.h>
/* https://cirosantilli.com/linux-kernel-module-cheat#mmap */

#define _XOPEN_SOURCE 700
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /* uintmax_t */
#include <string.h>
#include <sys/mman.h>
#include <unistd.h> /* sysconf */
#include <time.h>

#include "lkmc/pagemap.h" /* lkmc_pagemap_virt_to_phys_user */

#include "../common.h"
#include <signal.h>

// Logging
static unsigned long *log_buffs[NUM_LOG_BUFF];

/* Sense breaking signal */
static volatile sig_atomic_t keep_running = 1;
static void sig_handler(int _)
{
    (void)_;
    keep_running = 0;
}

static unsigned long get_nsecs(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000UL + ts.tv_nsec;
}

int main(int argc, char **argv) {

    /*  */
    // cpu_set_t set;
    // CPU_ZERO(&set);        // clear cpu mask
    // CPU_SET(2, &set);      // set cpu 0
    // sched_setaffinity(0, sizeof(cpu_set_t), &set);  // 0 is the calling process


    int fd;
    long page_size;
    char *address1/* , *address2 */;
    char buff_read[BUFFER_SIZE];
    // uintptr_t paddr;

    /* Allocate mem for log */
    // unsigned long *log_time_stamps = (unsigned long *) malloc(MAX_LOG_ENTRY*8);
    // if (!log_time_stamps) {
    //     printf("Malloc log_time_stamps failed\n!");
    //     return -1;
    // }
    // memset(log_time_stamps, 0, MAX_LOG_ENTRY*8);
    // for (int i=0; i<MAX_LOG_ENTRY; i++) {
    //     if (log_time_stamps[i] != 0) {
    //         printf("memset log_time_stamps failed\n!");
    //         return -1;
    //     }
    // }

    /*  */
    /* Alloc memory for log buffers */
    unsigned long *r;
    for (int i=0; i<NUM_LOG_BUFF; i++) {
        r =  (unsigned long *) malloc(MAX_ENTRIES_PER_LOG_BUFF*8);
        if (!r) {
            // error
            printf("[ERROR: malloc(MAX_ENTRIES_PER_LOG_BUFF*8) i[%d] of %d max_entries[%d]!\n", i, NUM_LOG_BUFF, MAX_ENTRIES_PER_LOG_BUFF);
            for (int j=0; j<i; j++) {
                free(log_buffs[j]);
            }
            return -1;
        }
        log_buffs[i] = r;
    }
    printf("Allocated NUM_LOG_BUFF[%d] for MAX_LOG_ENTRY[%d] packets!\n", NUM_LOG_BUFF, MAX_LOG_ENTRY);

    /*  */

    /* Open proc file */
    char name_buff[128];
    memset(name_buff, '\0', 100);
    memcpy(name_buff, path_prefix, strlen(path_prefix));
    memcpy(name_buff+strlen(path_prefix), proc_filename, strlen(proc_filename));
    printf("open pathname = %s\n", name_buff);
    fd = open(name_buff, O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open");
        assert(0);
    }
    printf("fd = %d\n", fd);
    /* Map mem */
    page_size = sysconf(_SC_PAGE_SIZE);
    address1 = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (address1 == MAP_FAILED) {
        perror("mmap");
        assert(0);
    }

    signal(SIGINT, sig_handler);
    // unsigned long count_log = 0;
    // unsigned long count_i = 0;
    while(keep_running) {
        // count_i += 1;
        // if ((count_i%USER_PROCESSING_RATE)!=0)
        //     continue;

        memset(buff_read, '\0', BUFFER_SIZE);
        /* ssize_t r =  */pread(fd, buff_read, BUFFER_SIZE, 0);
        for (int i=0; i<PKTS_PER_BUFFER; i++) {
            if (buff_read[i*PKT_BUFFER_SIZE] == '\0')
                continue;
            uint64_t now = get_nsecs();
            // Extract data from packet
            struct Payload pl;
            memcpy(&pl, buff_read+i*PKT_BUFFER_SIZE, sizeof(struct Payload));
            /* Check packet */
            if (pl.client_uid!=0 && pl.type==PL_DATA && pl.created_time!=0) {
                // pl.us_time_arrival_1 = now;
    #ifdef DEBUG_UP_INCOMING_PACKETS
                printf("-------------------------------------------------------\n");
                printf("Client_id[%lu] uid[%lu] type[%lu] create_time[%lu] delta[%lu usec]\n",
                    pl.client_uid, pl.uid, pl.type, pl.created_time, (now-pl.ks_time_arrival_2)/1000);
                printf("             ks_1[%lu] ks_2[%lu] us_1[%lu] us_2[%lu]\n", 
                    pl.ks_time_arrival_1, pl.ks_time_arrival_2,
                    pl.us_time_arrival_1, pl.us_time_arrival_2);
    #endif
                // Add timestamp to log at uid
                if (pl.uid <MAX_LOG_ENTRY && pl.uid >= 0) {
                    // log_time_stamps[pl.uid] = now;
                    unsigned int buff_index = pl.uid / MAX_ENTRIES_PER_LOG_BUFF;
                    log_buffs[buff_index][pl.uid % MAX_ENTRIES_PER_LOG_BUFF] = now;
                    // count_log += 1;
                } else {
                    printf("Something wrong [%lu]: pl.uid <MAX_LOG_ENTRY && pl.uid >= 0\n", pl.uid);
                }
            } else {
                printf("pl.client_uid[%lu], pl.type[%lu], pl.created_time[%lu]\n", pl.client_uid, pl.type, pl.created_time);
            }
        }
        // usleep(1);
    }

    /* Export log to text file */
    printf("\nExporting log file ...\n");   // enter new line to avoid the Ctrl+C (^C) char
    FILE *fp;
    // char buf[128];
    fp = fopen(path_km_user, "w");
    unsigned long zeroed = 0;
    for (unsigned long i=0; i<MAX_LOG_ENTRY; i++) {
        /* 
        memset(buf, '\0', 128);
        if (log_time_stamps[i] == 0)
            zeroed += 1;
        snprintf(buf, 100, "%lu\n", log_time_stamps[i]);
        fprintf(fp, "%s", buf);
         */

        unsigned int buff_index = i / MAX_ENTRIES_PER_LOG_BUFF;
        if (log_buffs[buff_index][i % MAX_ENTRIES_PER_LOG_BUFF] == 0)
            zeroed += 1;
        fprintf(fp, "%lu\n", log_buffs[buff_index][i % MAX_ENTRIES_PER_LOG_BUFF]);
    }
    fclose(fp);
    printf("Zeroed: %lu\n", zeroed);
    close(fd);

    for (int i=0; i<NUM_LOG_BUFF; i++) {
        free(log_buffs[i]);
    }

    return EXIT_SUCCESS;
}
