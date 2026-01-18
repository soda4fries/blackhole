package com.soda4fries.blackhole;

import java.io.IOException;
import java.io.InputStream;
import java.lang.foreign.Arena;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

/**
 * Simple wrapper for the Blackhole IP blocker C library.
 *
 * This class is thread-safe and ensures singleton behavior - only one instance
 * can be initialized at a time.
 */
public class Blackhole {

    private static final String RESOURCE_BASE = "/native/linux-x86_64/";
    private static final Object LOCK = new Object();

    private static Path tcProgPath;
    private static Path xdpProgPath;
    private static volatile boolean initialized = false;
    private static String currentInterface;

    /**
     * Initialize the blackhole IP blocker.
     * Automatically extracts BPF programs from bundled resources.
     * Can only be called once - subsequent calls will throw an exception.
     *
     * @param ifname Network interface name (e.g., "eth0")
     * @return 0 on success, -1 on error
     * @throws IllegalStateException if already initialized
     * @throws RuntimeException if BPF programs cannot be extracted or initialization fails
     */
    public static int init(String ifname) {
        synchronized (LOCK) {
            if (initialized) {
                throw new IllegalStateException(
                    "Blackhole already initialized on interface: " + currentInterface +
                    ". Call cleanup() first to reinitialize."
                );
            }

            try {
                extractBpfPrograms();
                int result = initInternal(ifname, tcProgPath.toString(), xdpProgPath.toString());

                if (result == 0) {
                    initialized = true;
                    currentInterface = ifname;
                }

                return result;
            } catch (IOException e) {
                throw new RuntimeException("Failed to extract BPF programs", e);
            }
        }
    }

    /**
     * Check if Blackhole has been initialized.
     *
     * @return true if initialized, false otherwise
     */
    public static boolean isInitialized() {
        return initialized;
    }

    /**
     * Ensure that Blackhole has been initialized before calling other methods.
     *
     * @throws IllegalStateException if not initialized
     */
    private static void ensureInitialized() {
        if (!initialized) {
            throw new IllegalStateException(
                "Blackhole not initialized. Call init(interfaceName) first."
            );
        }
    }

    /**
     * Internal initialization method with explicit BPF program paths.
     */
    private static int initInternal(String ifname, String tcProgPath, String xdpProgPath) {
        try (var arena = Arena.ofConfined()) {
            return Blackhole_h.blackhole_init(
                arena.allocateFrom(ifname),
                arena.allocateFrom(tcProgPath),
                arena.allocateFrom(xdpProgPath)
            );
        }
    }

    /**
     * Extract BPF programs from JAR resources to temporary files.
     */
    private static synchronized void extractBpfPrograms() throws IOException {
        if (tcProgPath != null && xdpProgPath != null) {
            return; // Already extracted
        }

        tcProgPath = extractResource(RESOURCE_BASE + "tc_egress.o", "tc_egress", ".o");
        xdpProgPath = extractResource(RESOURCE_BASE + "xdp_ingress.o", "xdp_ingress", ".o");

        // Mark for deletion on JVM exit
        tcProgPath.toFile().deleteOnExit();
        xdpProgPath.toFile().deleteOnExit();
    }

    /**
     * Extract a resource from the JAR to a temporary file.
     */
    private static Path extractResource(String resourcePath, String prefix, String suffix) throws IOException {
        try (InputStream in = Blackhole.class.getResourceAsStream(resourcePath)) {
            if (in == null) {
                throw new IOException("Resource not found: " + resourcePath +
                    ". Make sure to run './gradlew build' to bundle native resources.");
            }

            Path tempFile = Files.createTempFile(prefix, suffix);
            Files.copy(in, tempFile, StandardCopyOption.REPLACE_EXISTING);
            return tempFile;
        }
    }

    /**
     * Add an IP address to the whitelist.
     *
     * @param ip IP address in dotted-decimal notation (e.g., "192.168.1.1")
     * @return 0 on success, -1 on error
     * @throws IllegalStateException if not initialized
     */
    public static int addWhitelistIp(String ip) {
        ensureInitialized();

        try (var arena = Arena.ofConfined()) {
            return Blackhole_h.blackhole_add_whitelist_ip(
                arena.allocateFrom(ip)
            );
        }
    }

    /**
     * Clear all IP addresses from the whitelist.
     *
     * @return 0 on success, -1 on error
     * @throws IllegalStateException if not initialized
     */
    public static int clearWhitelist() {
        ensureInitialized();
        return Blackhole_h.blackhole_clear_whitelist.makeInvoker().apply();
    }

    /**
     * Cleanup and detach all BPF programs.
     * Clears the whitelist map, detaches XDP and TC programs, and resets all state.
     * After cleanup, you can call init() again if needed.
     *
     * @throws IllegalStateException if not initialized
     */
    public static void cleanup() {
        synchronized (LOCK) {
            ensureInitialized();
            Blackhole_h.blackhole_cleanup.makeInvoker().apply();
            initialized = false;
            currentInterface = null;
        }
    }
}
