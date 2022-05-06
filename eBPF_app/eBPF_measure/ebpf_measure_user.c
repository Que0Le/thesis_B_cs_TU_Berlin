/* SPDX-License-Identifier: GPL-2.0 */

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/resource.h>

#include <bpf/bpf.h>
#include <bpf/xsk.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if_link.h>
#include <linux/if_ether.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>


#include "../common/common_params.h"
#include "../common/common_user_bpf_xdp.h"
#include "../common/common_libbpf.h"

#include "/home/que/Desktop/mymodule/common.h"
int time_xsks_map_fd;
// unsigned long *log_time_stamps;
int pkt_map_fd;

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

// Logging
static unsigned long *log_buffs[NUM_LOG_BUFF];

#define NANOSEC_PER_SEC 1000000000 /* 10^9 */
static uint64_t gettime(void)
{
	struct timespec t;
	int res;

	res = clock_gettime(CLOCK_MONOTONIC, &t);
	if (res < 0) {
		fprintf(stderr, "Error with gettimeofday! (%i)\n", res);
		exit(EXIT_FAIL);
	}
	return (uint64_t) t.tv_sec * NANOSEC_PER_SEC + t.tv_nsec;
}

static const char *__doc__ = "XDP measure, based on XDP_tutorial's AF_XDP kernel bypass example\n";

static const struct option_wrapper long_options[] = {

	{{"help",	 no_argument,		NULL, 'h' },
	 "Show help", false},

	{{"dev",	 required_argument,	NULL, 'd' },
	 "Operate on device <ifname>", "<ifname>", true},

	{{"skb-mode",	 no_argument,		NULL, 'S' },
	 "Install XDP program in SKB (AKA generic) mode"},

	{{"native-mode", no_argument,		NULL, 'N' },
	 "Install XDP program in native mode"},

	{{"auto-mode",	 no_argument,		NULL, 'A' },
	 "Auto-detect SKB or native mode"},

	{{"force",	 no_argument,		NULL, 'F' },
	 "Force install, replacing existing program on interface"},

	{{"copy",        no_argument,		NULL, 'c' },
	 "Force copy mode"},

	{{"zero-copy",	 no_argument,		NULL, 'z' },
	 "Force zero-copy mode"},

	{{"queue",	 required_argument,	NULL, 'Q' },
	 "Configure interface receive queue for AF_XDP, default=0"},

	{{"poll-mode",	 no_argument,		NULL, 'p' },
	 "Use the poll() API waiting for packets to arrive"},

	{{"unload",      no_argument,		NULL, 'U' },
	 "Unload XDP program instead of loading"},

	{{"quiet",	 no_argument,		NULL, 'q' },
	 "Quiet mode (no output)"},

	{{"filename",    required_argument,	NULL,  1  },
	 "Load program from <file>", "<file>"},

	{{"progsec",	 required_argument,	NULL,  2  },
	 "Load program in <section> of the ELF file", "<section>"},

	{{0, 0, NULL,  0 }, NULL, false}
};

static bool global_exit;

static inline __sum16 csum16_add(__sum16 csum, __be16 addend)
{
	uint16_t res = (uint16_t)csum;

	res += (__u16)addend;
	return (__sum16)(res + (res < (__u16)addend));
}

static inline __sum16 csum16_sub(__sum16 csum, __be16 addend)
{
	return csum16_add(csum, ~addend);
}

static inline void csum_replace2(__sum16 *sum, __be16 old, __be16 new)
{
	*sum = ~csum16_add(csum16_sub(~(*sum), old), new);
}

static bool fetch_and_process_packet(unsigned int next_uid)
{
	struct Payload map_pl;	//map payload
	if (bpf_map_lookup_elem(pkt_map_fd, &next_uid, &map_pl) == 0) {
		if (map_pl.client_uid!=0 && map_pl.type==PL_DATA && map_pl.created_time!=0) {
			unsigned long now = gettime();
			// printf("__map: uid[%u] map_pl uid[%lu]\n", next_uid, map_pl.uid);
			if (map_pl.uid <MAX_LOG_ENTRY && map_pl.uid >= 0) {
				// log_time_stamps[map_pl.uid] = now;
				unsigned int buff_index = map_pl.uid / MAX_ENTRIES_PER_LOG_BUFF;
				log_buffs[buff_index][map_pl.uid % MAX_ENTRIES_PER_LOG_BUFF] = now;
#ifdef DEBUG_EBPF_INCOMING_PACKETS
				/* Calc diff time */
				unsigned long v = 0;
				if (bpf_map_lookup_elem(time_xsks_map_fd, &next_uid, &v) == 0) {
					printf("uid[%lu] diff: %lu\n", map_pl.uid, now-v);
				}
#endif
				return true;
			}
		}
	}
	return false;
}

static void exit_application(int signal)
{
	signal = signal;
	global_exit = true;
}

int main(int argc, char **argv)
{
	int ret;
	struct config cfg = {
		.ifindex   = -1,
		.do_unload = false,
		.filename = "",
		.progsec = "xdp_sock"
	};

	struct bpf_object *bpf_obj = NULL;

	/* Global shutdown handler */
	signal(SIGINT, exit_application);

	/* Cmdline options can change progsec */
	parse_cmdline_args(argc, argv, long_options, &cfg, "__doc__");

	/* Required option */
	if (cfg.ifindex == -1) {
		fprintf(stderr, "ERROR: Required option --dev missing\n\n");
		usage(argv[0], "__doc__", long_options, (argc == 1));
		return EXIT_FAIL_OPTION;
	}

	/* Unload XDP program if requested */
	if (cfg.do_unload)
		return xdp_link_detach(cfg.ifindex, cfg.xdp_flags, 0);

	/* Load custom program if configured */
	if (cfg.filename[0] != 0) {
		struct bpf_map *time_map;
		struct bpf_map *pkt_map;

		bpf_obj = load_bpf_and_xdp_attach(&cfg);
		if (!bpf_obj) {
			/* Error handling done in load_bpf_and_xdp_attach() */
			exit(EXIT_FAILURE);
		}

		/* Time Log map */
		time_map = bpf_object__find_map_by_name(bpf_obj, "uid_timestamps");
		time_xsks_map_fd = bpf_map__fd(time_map);
		if (time_xsks_map_fd < 0) {
			fprintf(stderr, "ERROR: no xsks map found: %s\n",
				strerror(time_xsks_map_fd));
			exit(EXIT_FAILURE);
		}

		/* Pkt map */
		pkt_map = bpf_object__find_map_by_name(bpf_obj, "pkt_payload");
		pkt_map_fd = bpf_map__fd(pkt_map);
		if (pkt_map_fd < 0) {
			fprintf(stderr, "ERROR: no xsks map found: %s\n",
				strerror(pkt_map_fd));
			exit(EXIT_FAILURE);
		}
	}

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

	/* Fetch packets */
	unsigned int next_uid = 0;
	while(!global_exit) {
		if (fetch_and_process_packet(next_uid)) {
			next_uid++;
		}
	}

	/* Export log file for userspace*/
	printf("\nExporting log file ...\n");   // enter new line to avoid the Ctrl+C (^C) char
    FILE *fp;
    fp = fopen(path_ebpf_us, "w");
	unsigned long zeroed = 0;
    for (unsigned long i=0; i<MAX_LOG_ENTRY; i++) {
		// if (log_time_stamps[i] == 0) {
		// 	printf("something fucked up with log_time_stamps uid[%lu]\n", i);
		// }
        // fprintf(fp, "%lu\n", log_time_stamps[i]);

        unsigned int buff_index = i / MAX_ENTRIES_PER_LOG_BUFF;
        if (log_buffs[buff_index][i % MAX_ENTRIES_PER_LOG_BUFF] == 0)
            zeroed += 1;
        fprintf(fp, "%lu\n", log_buffs[buff_index][i % MAX_ENTRIES_PER_LOG_BUFF]);
    }
    fclose(fp);

    for (int i=0; i<NUM_LOG_BUFF; i++) {
        free(log_buffs[i]);
    }
	// free(log_time_stamps);

	/* Export log file for kernel space (from map) */
	FILE *fp2;
	fp2 = fopen(path_ebpf_kern, "w");
	unsigned long v = 0;
	for (unsigned i=0; i<MAX_LOG_ENTRY; i++) {
		if (bpf_map_lookup_elem(time_xsks_map_fd, &i, &v) == 0) {
			fprintf(fp2, "%lu\n", v);
			// if (v!=0) {
				// printf("uid[%d] ks[%lu]\n", i, v);
			// }
		} else {
			printf("Error reading from time log map with bpf_map_lookup_elem!\n");
		}
	}
	printf("Zeroed: %lu\n", zeroed);
	fclose(fp2);

	/* Cleanup */
	xdp_link_detach(cfg.ifindex, cfg.xdp_flags, 0);

	return EXIT_OK;
}
