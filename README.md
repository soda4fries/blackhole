# Blackhole IP Blocker

Linux eBPF-based IP whitelisting library with Java bindings.

XDP hook on rx path to drop traffic not in whitelist

TC hook on tx path to add outgoing traffic to shared whitelist to allow return packets.

## Dependencies

```bash
sudo apt install -y build-essential cmake clang gcc-multilib libbpf-dev libelf-dev pkg-config openjdk-25-jdk linux-headers-$(uname -r)
```

## Build

```bash
./gradlew build
```

Output: `build/libs/blackhole-1.0.0.jar`

## Usage

### Java Library

```java
Blackhole.init("eth0");
Blackhole.addWhitelistIp("192.168.1.1");
Blackhole.clearWhitelist();
Blackhole.cleanup();
```

### Standalone Binary (ip_blocker)

**Attach to interface:**
```bash
sudo build/resources/main/native/linux-x86_64/ip_blocker eth0 192.168.1.1 192.168.1.2
```

**List eBPF maps:**
```bash
sudo bpftool map list
sudo bpftool map dump name whitelist_map
```

**View trace output:**
```bash
sudo cat /sys/kernel/debug/tracing/trace_pipe
```

## Clean

```bash
./gradlew clean      # Clean build artifacts
./gradlew cleanAll   # Remove everything including jextract
```
