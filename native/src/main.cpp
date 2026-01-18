#include <iostream>
#include <string>
#include <csignal>
#include <unistd.h>
#include "blackhole.h"

void signal_handler(int signum)
{
    std::cout << "\nReceived signal, cleaning up...\n";
    blackhole_cleanup();
    std::cout << "Done.\n";
    exit(0);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <interface> [whitelist IPs...]\n";
        return 1;
    }

    const char *ifname = argv[1];

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize blackhole */
    if (blackhole_init(ifname, "tc_egress.o", "xdp_ingress.o") != 0) {
        std::cerr << "Error: Failed to initialize blackhole\n";
        return 1;
    }

    /* Add initial whitelist IPs */
    if (argc > 2) {
        for (int i = 2; i < argc; i++) {
            if (blackhole_add_whitelist_ip(argv[i]) != 0) {
                std::cerr << "Warning: Failed to add IP '" << argv[i] << "' to whitelist\n";
            } else {
                std::cout << "Added " << argv[i] << " to whitelist\n";
            }
        }
    }

    std::cout << "Programs attached. Press Ctrl+C to exit.\n";

    /* Keep running until signal */
    while (true) {
        pause();  // Sleep until signal
    }

    return 0;
}

