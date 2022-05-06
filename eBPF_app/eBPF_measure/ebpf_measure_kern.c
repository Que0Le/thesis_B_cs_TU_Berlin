/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/bpf.h>

#include <bpf/bpf_helpers.h>

#include <stddef.h>
#include <linux/bpf.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#include <string.h>
#include "/home/que/Desktop/mymodule/common.h"
#include <errno.h>

/* header parse */
// The parsing helper functions from the packet01 lesson have moved here
#include "../common/parsing_helpers.h"

struct bpf_map_def SEC("maps") uid_timestamps = {
	.type = BPF_MAP_TYPE_ARRAY,
	.key_size = sizeof(unsigned int),
	.value_size = sizeof(unsigned long),
	.max_entries = 1 + MAX_LOG_ENTRY, 
};

struct bpf_map_def SEC("maps") pkt_payload = {
	.type = BPF_MAP_TYPE_ARRAY,
	.key_size = sizeof(unsigned int),
	.value_size = sizeof(struct Payload),
	.max_entries = 1 + MAX_LOG_ENTRY, 
};

struct bpf_map_def SEC("maps") xdp_stats_map = {
	.type        = BPF_MAP_TYPE_PERCPU_ARRAY,
	.key_size    = sizeof(int),
	.value_size  = sizeof(__u32),
	.max_entries = 64,
};

SEC("xdp_sock")
int xdp_sock_prog(struct xdp_md *ctx)
{
    int index = ctx->rx_queue_index;
    __u32 *pkt_count;
    void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
    
    /* parsing. From packet-solutions/xdp_prog_kern_02.c */
    int eth_type, ip_type;
	struct ethhdr *eth;
	struct iphdr *iphdr;
	struct udphdr *udphdr;
	struct hdr_cursor nh = { .pos = data };

	eth_type = parse_ethhdr(&nh, data_end, &eth);
	if (eth_type != bpf_htons(ETH_P_IP)) {
	    return XDP_PASS;
	}
    ip_type = parse_iphdr(&nh, data_end, &iphdr);
	if (ip_type != IPPROTO_UDP) {
        return XDP_PASS;
	}
    if (parse_udphdr(&nh, data_end, &udphdr) != sizeof(struct Payload)) {
        return XDP_PASS;
    }
    // bpf_printk("test: %d\n", data_end-data);
    // bpf_printk("test: %d, port: %u, len: %u\n", data_end-data, udphdr->dest, udphdr->len);
    // udphdr->dest = bpf_htons(bpf_ntohs(udphdr->dest) - 1);
    if (udphdr->dest != bpf_htons(DEST_PORT)) {
        return XDP_PASS;
    }
    /* End parsing */

    /*  */
    if (data_end >= data + 106) {
        if (data_end >= data+42+sizeof(struct Payload)) {
            unsigned long now = bpf_ktime_get_ns();
            struct Payload *pl;
            pl = data+42;
            unsigned long *time_in_map = bpf_map_lookup_elem(&uid_timestamps, &pl->uid);
            if (time_in_map) {
                // bpf_printk("!! uid before: %lu\n", *value);
                *time_in_map = now;
    #ifdef DEBUG_EBPF_INCOMING_PACKETS
                if (pl->uid % 10 == 0) {
                    bpf_printk("!! uid[%lu] timestamp after : %lu\n", pl->uid, *time_in_map);
                }
    #endif
            }

            /* Copying payload data*/
            /* 
            // This is one way to do that ...
            struct Payload *pl_in_map;
            pl_in_map = bpf_map_lookup_elem(&pkt_payload, &pl->uid);
            if (pl_in_map) {
                memcpy(pl_in_map, data+42, sizeof(struct Payload));
                struct Payload *pl_temp = bpf_map_lookup_elem(&pkt_payload, &pl->uid);
                if (pl_temp)
                    bpf_printk("!! uid[%lu] memcpied in map!\n", pl_temp->uid);
            }    
            */
            // The good way is to use update_elem
            int r = bpf_map_update_elem(&pkt_payload, &pl->uid, pl, BPF_ANY);
            if (r==-1) {
                bpf_printk("Failed add to pkt_payload uid[%lu]: ", pl->uid);
            }
    #ifdef DEBUG_EBPF_INCOMING_PACKETS
            // Check real value in map. Not important
            struct Payload *pl_temp = bpf_map_lookup_elem(&pkt_payload, &pl->uid);
            if (pl_temp)
                bpf_printk("!! uid[%lu] memcpied in map!\n", pl_temp->uid);
    #endif
            /* Counting */
            pkt_count = bpf_map_lookup_elem(&xdp_stats_map, &index);
            if (pkt_count) {
                (*pkt_count)++;
            }
            return XDP_PASS;
        }
        return XDP_PASS;
    }
    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
