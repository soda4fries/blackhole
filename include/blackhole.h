#ifndef BLACKHOLE_H
#define BLACKHOLE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the blackhole IP blocker
 * @param ifname Network interface name (e.g., "eth0")
 * @param tc_prog_path Path to tc_egress.o BPF program
 * @param xdp_prog_path Path to xdp_ingress.o BPF program
 * @return 0 on success, -1 on error
 */
int blackhole_init(const char *ifname, const char *tc_prog_path, const char *xdp_prog_path);

/**
 * Add an IP address to the whitelist
 * @param ip_str IP address in dotted-decimal notation (e.g., "192.168.1.1")
 * @return 0 on success, -1 on error
 */
int blackhole_add_whitelist_ip(const char *ip_str);

/**
 * Clear all IP addresses from the whitelist
 * @return 0 on success, -1 on error
 */
int blackhole_clear_whitelist();

/**
 * Cleanup and detach all BPF programs
 * Clears the whitelist map, detaches XDP and TC programs, and resets all state
 */
void blackhole_cleanup();

#ifdef __cplusplus
}
#endif

#endif /* BLACKHOLE_H */
