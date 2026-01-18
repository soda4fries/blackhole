# Blackhole IP Blocker

Linux eBPF-based IP whitelisting library with Java bindings.

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

```java
Blackhole.init("eth0");
Blackhole.addWhitelistIp("192.168.1.1");
Blackhole.clearWhitelist();
Blackhole.cleanup();
```

## Clean

```bash
./gradlew clean      # Clean build artifacts
./gradlew cleanAll   # Remove everything including jextract
```
