cmake_minimum_required(VERSION 3.10)
project(blackhole)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ARCH "x86_64") # default architecture

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBBPF REQUIRED libbpf)

# Include directories for userspace program
include_directories(
    ${CMAKE_SOURCE_DIR}/../shared  # Public API header
    ${CMAKE_SOURCE_DIR}/include    # Internal headers (common.h)
    ${LIBBPF_INCLUDE_DIRS}         # libbpf helpers
)

# eBPF program sources
set(TC_BPF_SOURCE ${CMAKE_SOURCE_DIR}/src/tc_egress.c)
set(XDP_BPF_SOURCE ${CMAKE_SOURCE_DIR}/src/xdp_ingress.c)
set(TC_BPF_OBJECT ${CMAKE_BINARY_DIR}/tc_egress.o)
set(XDP_BPF_OBJECT ${CMAKE_BINARY_DIR}/xdp_ingress.o)

# Custom command to compile TC eBPF program
add_custom_command(
    OUTPUT ${TC_BPF_OBJECT}
    COMMAND clang -O2 -g -target bpf -D__TARGET_ARCH_${ARCH} -c ${TC_BPF_SOURCE}
        -I${CMAKE_SOURCE_DIR}/include
        -I${LIBBPF_INCLUDE_DIRS}
        -o ${TC_BPF_OBJECT}
    DEPENDS ${TC_BPF_SOURCE} ${CMAKE_SOURCE_DIR}/include/common.h
    COMMENT "Compiling TC eBPF program"
)

# Custom command to compile XDP eBPF program
add_custom_command(
    OUTPUT ${XDP_BPF_OBJECT}
    COMMAND clang -O2 -g -target bpf -D__TARGET_ARCH_${ARCH} -c ${XDP_BPF_SOURCE}
            -I${CMAKE_SOURCE_DIR}/include
            -I${LIBBPF_INCLUDE_DIRS}
            -o ${XDP_BPF_OBJECT}
    DEPENDS ${XDP_BPF_SOURCE} ${CMAKE_SOURCE_DIR}/include/common.h
    COMMENT "Compiling XDP eBPF program"
)

# Custom target for both eBPF objects
add_custom_target(bpf_programs ALL DEPENDS ${TC_BPF_OBJECT} ${XDP_BPF_OBJECT})

# C++ shared library
add_library(blackhole SHARED src/ip_blocker.cpp)

# Set library properties
set_target_properties(blackhole PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    PUBLIC_HEADER ${CMAKE_SOURCE_DIR}/../shared/blackhole.h
)

# Link libraries for shared library
target_link_libraries(blackhole
    ${LIBBPF_LIBRARIES}
    elf
    z
)

# Ensure eBPF programs are built before library
add_dependencies(blackhole bpf_programs)

# Terminal executable
add_executable(ip_blocker src/main.cpp)

# Link executable against the shared library
target_link_libraries(ip_blocker blackhole)

# Ensure library is built before executable
add_dependencies(ip_blocker blackhole)

# Install targets
install(TARGETS blackhole ip_blocker
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)
install(FILES ${TC_BPF_OBJECT} ${XDP_BPF_OBJECT} DESTINATION share/blackhole)

# Print build info
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "libbpf include dirs: ${LIBBPF_INCLUDE_DIRS}")
message(STATUS "libbpf libraries: ${LIBBPF_LIBRARIES}")
message(STATUS "Target architecture: ${ARCH}")
