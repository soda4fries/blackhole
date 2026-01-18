#include <linux/bpf.h>          // for BPF types
#include <linux/if_ether.h>     // Ethernet headers
#include <linux/ip.h>           // IP headers
#include <linux/pkt_cls.h>      // TC helper structs
#include <bpf/bpf_helpers.h>    // libbpf helpers
#include <bpf/bpf_endian.h>     // bpf_htons, etc.
#include "common.h"



/* Shared whitelist map (same as XDP program) */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, WHITELIST_MAP_SIZE);
    __type(key, __u32);    /* IPv4 address */
    __type(value, __u8);   /* Allowed flag */
    __uint(pinning, LIBBPF_PIN_BY_NAME);
} whitelist_map SEC(".maps");


SEC("tc")
int tc_egress_func(struct __sk_buff *skb)
{
    void *data_end = (void *)(long)skb->data_end;
    void *data = (void *)(long)skb->data;

    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return TC_ACT_OK;

    if (eth->h_proto != bpf_htons(ETH_P_IP))
        return TC_ACT_OK;

    struct iphdr *ip = (void *)(eth + 1);
    if ((void *)(ip + 1) > data_end)
        return TC_ACT_OK;

    __u32 dst_ip = ip->daddr;

    // Check if the IP is already in the map
    __u8 *existing = bpf_map_lookup_elem(&whitelist_map, &dst_ip);
    if (!existing) {
        __u8 allowed = 1;
        bpf_map_update_elem(&whitelist_map, &dst_ip, &allowed, BPF_ANY);

        // Print new IPs only
        bpf_printk("TC EGRESS: added %d.%d.%d.%d to whitelist\n",
                   dst_ip & 0xff,
                   (dst_ip >> 8) & 0xff,
                   (dst_ip >> 16) & 0xff,
                   (dst_ip >> 24) & 0xff);
    }

    return TC_ACT_OK;
}


char _license[] SEC("license") = "GPL";
