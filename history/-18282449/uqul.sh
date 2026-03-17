#!/bin/bash

# Model Blob Compression and Segmentation Script
# Reduces storage footprint by compressing model files into chunks

set -e

MODEL_FILE="$1"
OUTPUT_DIR="$2"
CHUNK_SIZE="${3:-1G}"

if [ -z "$MODEL_FILE" ] || [ -z "$OUTPUT_DIR" ]; then
    echo "Usage: $0 <model_file> <output_dir> [chunk_size]"
    echo "Example: $0 bigdaddyg-40gb-model.gguf ./compressed_model 1G"
    exit 1
fi

echo "Compressing model: $MODEL_FILE"
echo "Output directory: $OUTPUT_DIR"
echo "Chunk size: $CHUNK_SIZE"

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Get model info
MODEL_SIZE=$(stat -f%z "$MODEL_FILE" 2>/dev/null || stat -c%s "$MODEL_FILE")
echo "Original model size: $(numfmt --to=iec $MODEL_SIZE)"

# Split model into chunks
echo "Splitting model into chunks..."
split -b "$CHUNK_SIZE" "$MODEL_FILE" "$OUTPUT_DIR/chunk_"

# Compress each chunk
echo "Compressing chunks..."
for chunk in "$OUTPUT_DIR"/chunk_*; do
    echo "Compressing $(basename "$chunk")..."
    gzip -9 "$chunk"
done

# Create metadata file
cat > "$OUTPUT_DIR/model_metadata.json" << EOF
{
    "original_file": "$(basename "$MODEL_FILE")",
    "original_size": $MODEL_SIZE,
    "chunk_size": "$CHUNK_SIZE",
    "chunks": [
EOF

# Add chunk information
FIRST=true
for chunk in "$OUTPUT_DIR"/chunk_*.gz; do
    if [ "$FIRST" = true ]; then
        FIRST=false
    else
        echo "," >> "$OUTPUT_DIR/model_metadata.json"
    fi
    CHUNK_SIZE=$(stat -f%z "$chunk" 2>/dev/null || stat -c%s "$chunk")
    echo "        {\"file\": \"$(basename "$chunk")\", \"compressed_size\": $CHUNK_SIZE}" >> "$OUTPUT_DIR/model_metadata.json"
done

cat >> "$OUTPUT_DIR/model_metadata.json" << EOF

    ]
}
EOF

# Calculate compression ratio
COMPRESSED_SIZE=$(find "$OUTPUT_DIR" -name "chunk_*.gz" -exec stat -f%z {} + 2>/dev/null || find "$OUTPUT_DIR" -name "chunk_*.gz" -exec stat -c%s {} + | awk '{sum+=$1} END {print sum}')
RATIO=$(echo "scale=2; $COMPRESSED_SIZE * 100 / $MODEL_SIZE" | bc)

echo "Compression complete!"
echo "Original size: $(numfmt --to=iec $MODEL_SIZE)"
echo "Compressed size: $(numfmt --to=iec $COMPRESSED_SIZE)"
echo "Compression ratio: $RATIO%"
echo "Space saved: $(numfmt --to=iec $((MODEL_SIZE - COMPRESSED_SIZE)))"