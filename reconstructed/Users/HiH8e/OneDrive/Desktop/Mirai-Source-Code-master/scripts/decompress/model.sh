#!/bin/bash

# Dynamic Model Decompression Script
# Reconstructs model from compressed chunks on-demand

set -e

COMPRESSED_DIR="$1"
OUTPUT_FILE="$2"
TEMP_DIR="${3:-/tmp/model_temp}"

if [ -z "$COMPRESSED_DIR" ] || [ -z "$OUTPUT_FILE" ]; then
    echo "Usage: $0 <compressed_dir> <output_file> [temp_dir]"
    echo "Example: $0 ./compressed_model bigdaddyg-40gb-model.gguf"
    exit 1
fi

METADATA_FILE="$COMPRESSED_DIR/model_metadata.json"

if [ ! -f "$METADATA_FILE" ]; then
    echo "Error: Metadata file not found: $METADATA_FILE"
    exit 1
fi

echo "Decompressing model from: $COMPRESSED_DIR"
echo "Output file: $OUTPUT_FILE"

# Create temp directory
mkdir -p "$TEMP_DIR"

# Read metadata
ORIGINAL_FILE=$(jq -r '.original_file' "$METADATA_FILE")
ORIGINAL_SIZE=$(jq -r '.original_size' "$METADATA_FILE")

echo "Reconstructing: $ORIGINAL_FILE"
echo "Expected size: $(numfmt --to=iec $ORIGINAL_SIZE)"

# Decompress chunks in order
echo "Decompressing chunks..."
> "$TEMP_DIR/reconstructed_model"

# Get sorted chunk list
CHUNKS=$(jq -r '.chunks[].file' "$METADATA_FILE" | sort)

for chunk in $CHUNKS; do
    echo "Processing $chunk..."
    gunzip -c "$COMPRESSED_DIR/$chunk" >> "$TEMP_DIR/reconstructed_model"
done

# Verify size
RECONSTRUCTED_SIZE=$(stat -f%z "$TEMP_DIR/reconstructed_model" 2>/dev/null || stat -c%s "$TEMP_DIR/reconstructed_model")

if [ "$RECONSTRUCTED_SIZE" -ne "$ORIGINAL_SIZE" ]; then
    echo "Error: Size mismatch!"
    echo "Expected: $ORIGINAL_SIZE"
    echo "Got: $RECONSTRUCTED_SIZE"
    exit 1
fi

# Move to final location
mv "$TEMP_DIR/reconstructed_model" "$OUTPUT_FILE"

# Cleanup
rm -rf "$TEMP_DIR"

echo "Decompression complete!"
echo "Model reconstructed: $OUTPUT_FILE"