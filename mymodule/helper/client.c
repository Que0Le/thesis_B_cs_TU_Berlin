// Client side implementation of UDP client-server model
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

#define MAXLINE 1024
#define PAYLOAD_SIZE 128

static unsigned long get_nsecs(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000UL + ts.tv_nsec;
}

// Driver code
int main() {
    int sockfd;
    // char buffer[MAXLINE];
    struct sockaddr_in     servaddr;

    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
        
    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(DEST_PORT);
    servaddr.sin_addr.s_addr = inet_addr(DEST_IPADDR);
    //servaddr.sin_addr.s_addr = INADDR_ANY;
    unsigned long uid = 0;
    unsigned long begin_sent = get_nsecs();

    struct Payload pl = {
        1, 0, PL_DATA, 0, 0, 0,0,0
    };

    unsigned long count_i = 0;
    unsigned long start = get_nsecs();
    unsigned long stop = 0;
    while(1) {

        if (uid > MAX_LOG_ENTRY) {
            if (stop == 0) {
                stop = get_nsecs();
            }
            unsigned long delta = MAX_LOG_ENTRY/((stop-start)/1000000UL);
            printf("Rate sent = %lu pkt/ms\n", delta);
            break;
            // usleep(100*1000);
        }
        /* Prepare payload */
        // bzero(buffer, PAYLOAD_SIZE);

        // unsigned long now = get_nsecs();
        pl.uid = uid;
        pl.created_time = get_nsecs();
        // memcpy(buffer, &pl, sizeof(struct Payload));
        int sent = sendto(sockfd, &pl, sizeof(struct Payload),
            MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
        if (uid % 1000 == 0) {
            printf("Hello message uid[%lu] ( %d procent) of MAX_LOG_ENTRY[%d] sent (%d of %zu bytes)\n", 
                uid, (int) uid*100/MAX_LOG_ENTRY, MAX_LOG_ENTRY, sent, sizeof(struct Payload));
        }
        uid += 1;
        //

        // memcpy(payload, hello, strlen(hello));
        // int written = sprintf(payload+strlen(hello), "uid[%lu] sent_at[%lu]", uid, now);
        // int n, len;
        // int len;
        // int sent = sendto(sockfd, (const char *)payload, strlen(payload),
        //     MSG_CONFIRM, (const struct sockaddr *) &servaddr, 
        //         sizeof(servaddr));
        // printf("Hello message [%lu] sent (%lu of %d bytes): %lu\n", uid, strlen(payload), sent, now);
        // uid += 1;

        // n = recvfrom(sockfd, (char *)buffer, MAXLINE, 
        //             MSG_WAITALL, (struct sockaddr *) &servaddr,
        //             &len);
        // buffer[n] = '\0';
        // printf("Server : %s\n", buffer);

        if ((CLIENT_RATE != 0) && (count_i % CLIENT_RATE == 0))
            usleep(1);
        count_i+=1;

    }

    close(sockfd);
    return 0;
}