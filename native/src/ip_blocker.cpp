#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <csignal>
#include <cerrno>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if_link.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "common.h"

static struct bpf_object *tc_obj = nullptr;
static struct bpf_object *xdp_obj = nullptr;
static int ifindex = -1;
static struct bpf_tc_hook tc_hook = {};
static struct bpf_tc_opts tc_opts = {};
static std::vector<std::string> whitelist_ips;
static int map_fd = -1;
static bool created_qdisc = false;  

static void clear_map(int fd)
{
    if (fd < 0) {
        return;
    }

    __u32 key = 0, next_key;

    while (bpf_map_get_next_key(fd, &key, &next_key) == 0) {
        bpf_map_delete_elem(fd, &next_key);
        key = next_key;
    }

    key = 0;
    bpf_map_delete_elem(fd, &key);
}

static void cleanup_internal(bool do_exit)
{
    if (map_fd >= 0) {
        clear_map(map_fd);
    }

    if (ifindex > 0) {
        int ret = bpf_xdp_detach(ifindex, 0, nullptr);
        if (ret != 0) {
            std::cerr << "Warning: XDP detach failed: " << strerror(-ret) << "\n";
        }
    }

    // Detach TC program properly
    if (tc_hook.ifindex > 0) {
        tc_opts.prog_fd = 0;
        tc_opts.prog_id = 0;
        tc_opts.flags = 0;

        int ret = bpf_tc_detach(&tc_hook, &tc_opts);
        if (ret != 0 && ret != -ENOENT) {
            std::cerr << "Warning: TC detach failed: " << strerror(-ret) << "\n";
        }

        // Only destroy the hook if we created it
        if (created_qdisc) {
            ret = bpf_tc_hook_destroy(&tc_hook);
            if (ret != 0 && ret != -ENOENT) {
                std::cerr << "Warning: TC hook destroy failed: " << strerror(-ret) << "\n";
            }
        }
    }

    if (tc_obj) {
        bpf_object__close(tc_obj);
        tc_obj = nullptr;
    }
    if (xdp_obj) {
        bpf_object__close(xdp_obj);
        xdp_obj = nullptr;
    }

    ifindex = -1;
    map_fd = -1;
    created_qdisc = false;
    memset(&tc_hook, 0, sizeof(tc_hook));
    memset(&tc_opts, 0, sizeof(tc_opts));
    whitelist_ips.clear();

    if (do_exit) {
        exit(0);
    }
}

void signal_handler(int signum)
{
    cleanup_internal(true);
}


int add_to_whitelist(int map_fd, const char *ip_str)
{
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        return -1;
    }

    __u32 ip = addr.s_addr;
    __u8 allowed = 1;

    if (bpf_map_update_elem(map_fd, &ip, &allowed, BPF_ANY) != 0) {
        return -1;
    }

    whitelist_ips.push_back(ip_str);
    return 0;
}

extern "C" {

void blackhole_cleanup();

int blackhole_init(const char *ifname, const char *tc_prog_path, const char *xdp_prog_path)
{
    if (!ifname || !tc_prog_path || !xdp_prog_path) {
        return -1;
    }

    ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        std::cerr << "Error: Interface '" << ifname << "' not found.\n";
        return -1;
    }

    /* Load TC egress program */
    tc_obj = bpf_object__open_file(tc_prog_path, nullptr);
    if (libbpf_get_error(tc_obj)) {
        std::cerr << "Error: Failed to open " << tc_prog_path << "\n";
        return -1;
    }

    if (bpf_object__load(tc_obj) != 0) {
        std::cerr << "Error: Failed to load TC program\n";
        bpf_object__close(tc_obj);
        return -1;
    }

    /* Load XDP ingress program */
    xdp_obj = bpf_object__open_file(xdp_prog_path, nullptr);
    if (libbpf_get_error(xdp_obj)) {
        std::cerr << "Error: Failed to open " << xdp_prog_path << "\n";
        bpf_object__close(tc_obj);
        return -1;
    }

    if (bpf_object__load(xdp_obj) != 0) {
        std::cerr << "Error: Failed to load XDP program\n";
        bpf_object__close(tc_obj);
        bpf_object__close(xdp_obj);
        return -1;
    }

    tc_hook.sz = sizeof(tc_hook);
    tc_hook.ifindex = ifindex;
    tc_hook.attach_point = BPF_TC_EGRESS;

    int ret = bpf_tc_hook_create(&tc_hook);
    if (ret == 0) {
        created_qdisc = true; 
    } else if (ret == -EEXIST) {
        created_qdisc = false;  
    } else {
        std::cerr << "Error: Failed to create TC hook\n";
        bpf_object__close(tc_obj);
        bpf_object__close(xdp_obj);
        return -1;
    }

    /* Find TC program */
    struct bpf_program *tc_prog = bpf_object__find_program_by_name(tc_obj, "tc_egress_func");
    if (!tc_prog) {
        std::cerr << "Error: TC program 'tc_egress_func' not found\n";
        bpf_tc_hook_destroy(&tc_hook);
        bpf_object__close(tc_obj);
        bpf_object__close(xdp_obj);
        return -1;
    }

    int tc_prog_fd = bpf_program__fd(tc_prog);
    if (tc_prog_fd < 0) {
        std::cerr << "Error: Failed to get TC program FD\n";
        bpf_tc_hook_destroy(&tc_hook);
        bpf_object__close(tc_obj);
        bpf_object__close(xdp_obj);
        return -1;
    }

    /* Attach TC program */
    tc_opts.sz = sizeof(tc_opts);
    tc_opts.prog_fd = tc_prog_fd;

    if (bpf_tc_attach(&tc_hook, &tc_opts) != 0) {
        std::cerr << "Error: Failed to attach TC program\n";
        bpf_tc_hook_destroy(&tc_hook);
        bpf_object__close(tc_obj);
        bpf_object__close(xdp_obj);
        return -1;
    }

    /* Find XDP program */
    struct bpf_program *xdp_prog = bpf_object__find_program_by_name(xdp_obj, "xdp_ingress_func");
    if (!xdp_prog) {
        std::cerr << "Error: XDP program 'xdp_ingress_func' not found\n";
        bpf_tc_hook_destroy(&tc_hook);
        bpf_object__close(tc_obj);
        bpf_object__close(xdp_obj);
        return -1;
    }

    int xdp_prog_fd = bpf_program__fd(xdp_prog);
    if (xdp_prog_fd < 0) {
        std::cerr << "Error: Failed to get XDP program FD\n";
        bpf_tc_hook_destroy(&tc_hook);
        bpf_object__close(tc_obj);
        bpf_object__close(xdp_obj);
        return -1;
    }

    /* Attach XDP program */
    if (bpf_xdp_attach(ifindex, xdp_prog_fd, 0, nullptr) != 0) {
        std::cerr << "Error: Failed to attach XDP program\n";
        bpf_tc_hook_destroy(&tc_hook);
        bpf_object__close(tc_obj);
        bpf_object__close(xdp_obj);
        return -1;
    }

    /* Get whitelist map (from either program, they share it) */
    struct bpf_map *whitelist_map = bpf_object__find_map_by_name(xdp_obj, "whitelist_map");
    if (!whitelist_map) {
        std::cerr << "Error: Whitelist map not found\n";
        blackhole_cleanup();
        return -1;
    }

    map_fd = bpf_map__fd(whitelist_map);
    if (map_fd < 0) {
        std::cerr << "Error: Failed to get whitelist map FD\n";
        blackhole_cleanup();
        return -1;
    }

    return 0;
}

int blackhole_add_whitelist_ip(const char *ip_str)
{
    if (map_fd < 0) {
        return -1;
    }

    return add_to_whitelist(map_fd, ip_str);
}

void blackhole_cleanup()
{
    cleanup_internal(false);
}

int blackhole_clear_whitelist()
{
    if (map_fd < 0) {
        return -1;
    }

    clear_map(map_fd);
    whitelist_ips.clear();
    return 0;
}

}
