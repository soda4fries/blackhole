#!/bin/bash
# File: extract_header.sh
# Description: Generate Java bindings from blackhole.h using jextract.

# Paths
JEXTRACT="./jextract-25/bin/jextract"
HEADER="./include/blackhole.h"
OUTPUT_DIR="./java-binding/src/main/java"  # Your Java source root
PACKAGE="com.soda4fries.blackhole"
HEADER_CLASS_NAME="Blackhole_h"           # Optional: main class name

# Ensure jextract exists
if [ ! -x "$JEXTRACT" ]; then
    echo "Error: jextract not found or not executable at $JEXTRACT"
    exit 1
fi

# Prepare output directory
mkdir -p "$OUTPUT_DIR"

# Run jextract
"$JEXTRACT" \
    --output "$OUTPUT_DIR" \
    --target-package "$PACKAGE" \
    --header-class-name "$HEADER_CLASS_NAME" \
    "$HEADER"

