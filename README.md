# Blackhole

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

### Terminal

```bash
sudo ./ip_blocker <interface> [whitelist_ips...]
```

Example:
```bash
sudo ./ip_blocker eth0 192.168.1.1 10.0.0.1
```

### Library

```c
#include <blackhole.h>

blackhole_init("eth0", "tc_egress.o", "xdp_ingress.o");
blackhole_add_whitelist_ip("192.168.1.1");
blackhole_cleanup();
```

Compile: `gcc -o app app.c -lblackhole`
