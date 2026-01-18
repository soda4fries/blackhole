#include <linux/bpf.h>          // for BPF types
#include <linux/if_ether.h>     // Ethernet headers
#include <linux/ip.h>           // IP headers
#include <linux/pkt_cls.h>      // TC helper structs
#include <bpf/bpf_helpers.h>    // libbpf helpers
#include <bpf/bpf_endian.h>     // bpf_htons, etc.
#include "common.h"


/* Shared whitelist map (same as TC program) */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, WHITELIST_MAP_SIZE);
    __type(key, __u32);    /* IPv4 address */
    __type(value, __u8);   /* Allowed flag */
    __uint(pinning, LIBBPF_PIN_BY_NAME);
} whitelist_map SEC(".maps");

SEC("xdp")
int xdp_ingress_func(struct xdp_md *ctx)
{
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return XDP_PASS;

    if (eth->h_proto != bpf_htons(ETH_P_IP))
        return XDP_PASS;

    struct iphdr *ip = (void *)(eth + 1);
    if ((void *)(ip + 1) > data_end)
        return XDP_PASS;

    __u32 src_ip = ip->saddr;
    __u8 *allowed = bpf_map_lookup_elem(&whitelist_map, &src_ip);

    if (allowed && *allowed) {
        // IP is whitelisted - allow
        // bpf_printk("XDP PASS: %d.%d.%d.%d\n",
        //            src_ip & 0xff,
        //            (src_ip >> 8) & 0xff,
        //            (src_ip >> 16) & 0xff,
        //            (src_ip >> 24) & 0xff);
        return XDP_PASS;
    }

    // Not in whitelist - drop
    bpf_printk("XDP DROP: %d.%d.%d.%d\n",
               src_ip & 0xff,
               (src_ip >> 8) & 0xff,
               (src_ip >> 16) & 0xff,
               (src_ip >> 24) & 0xff);

    return XDP_DROP;
}


char _license[] SEC("license") = "GPL";
