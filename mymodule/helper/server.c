// #define _GNU_SOURCE             /* See feature_test_macros(7) */
// #include <sched.h>
// Server side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
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

#define MAXLINE 1024
  
int main(int argc, char *argv[]) {
    /*  */
    // cpu_set_t set;
    // CPU_ZERO(&set);        // clear cpu mask
    // CPU_SET(1, &set);      // set cpu 0
    // sched_setaffinity(0, sizeof(cpu_set_t), &set);  // 0 is the calling process

    if (argc!=2) {
        printf("Specify log output!\n");
        exit(EXIT_FAILURE);
    }
    if ((strcmp("km", argv[1])!=0 && strcmp("ebpf", argv[1])!=0)) {
        printf("Log only for KM or EBPF\n");
        exit(EXIT_FAILURE);
    }

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

    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr, cliaddr;
        
    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
        
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
        
    // Filling server information
    servaddr.sin_family    = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(DEST_PORT);
        
    // Bind the socket with the server address
    if ( bind(sockfd, (const struct sockaddr *)&servaddr, 
            sizeof(servaddr)) < 0 )
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    signal(SIGTSTP, sig_handler);
    //Ctrl+C - SIGINT
    //Ctrl+\ - SIGQUIT
    //Ctrl+Z - SIGTSTP

    printf("Server listening ... Ctrl+Z to break while(). Ctrl+C to terminate the program.\n");
    // unsigned long count_log = 0;
    while(keep_running) {
        // if (count_log == MAX_LOG_ENTRY) {
        //     break;
        // }

        socklen_t len;
        bzero(buffer, MAXLINE);
        len = sizeof(cliaddr);  //len is value/resuslt

        recvfrom(sockfd, (char *)buffer, MAXLINE, 
                    MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                    &len);
        unsigned long now = get_nsecs();
        struct Payload pl;
        memcpy(&pl, buffer, sizeof(struct Payload));
        if (pl.uid <MAX_LOG_ENTRY && pl.uid >= 0) {
            // log_time_stamps[pl.uid] = now;
            unsigned long buff_index = pl.uid / MAX_ENTRIES_PER_LOG_BUFF;
            log_buffs[buff_index][pl.uid % MAX_ENTRIES_PER_LOG_BUFF] = now;
            // count_log += 1;
        }
#ifdef DEBUG_US_INCOMING_PACKETS
        printf("-------------------------------------------------------\n");
        printf("Client_id[%lu] uid[%lu] type[%lu] create_time[%lu]\n",
                pl.client_uid, pl.uid, pl.type, pl.created_time);
        printf("             ks_1[%lu] ks_2[%lu] us_1[%lu] us_2[%lu]\n", 
                pl.ks_time_arrival_1, pl.ks_time_arrival_2,
                pl.us_time_arrival_1, pl.us_time_arrival_2);
#endif
        if (pl.uid == (MAX_LOG_ENTRY -1)) {
            break;
        }
    }

    /* Export log to text file */
    printf("\nExporting log file ...\n");   // enter new line to avoid the Ctrl+C (^C) char
    unsigned long zeroed = 0;
    FILE *fp;
    if (strcmp("km", argv[1])==0) {
        fp = fopen(path_km_server_linuxsocket, "w");
    } else if (strcmp("ebpf", argv[1])==0) {
        fp = fopen(path_ebpf_server_linuxsocket, "w");
    }
    /* 
    for (unsigned long i=0; i<MAX_LOG_ENTRY; i++) {
        if (log_time_stamps[i] == 0)
            zeroed += 1;
        fprintf(fp, "%lu\n", log_time_stamps[i]);
    }
    */
    for (unsigned long i=0; i<MAX_LOG_ENTRY; i++) {
        unsigned long buff_index = i / MAX_ENTRIES_PER_LOG_BUFF;
        if (log_buffs[buff_index][i % MAX_ENTRIES_PER_LOG_BUFF] == 0)
            zeroed += 1;
        fprintf(fp, "%lu\n", log_buffs[buff_index][i % MAX_ENTRIES_PER_LOG_BUFF]);
    }

    fclose(fp);
    printf("Zeroed: %lu\n", zeroed);

    // free(log_time_stamps);
    for (int i=0; i<NUM_LOG_BUFF; i++) {
        free(log_buffs[i]);
    }

    return 0;
}