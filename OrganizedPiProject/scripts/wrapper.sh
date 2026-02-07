#!/bin/bash
docker run --rm \
  --cpus=".5" \
  --memory="256m" \
  --network=none \
  --read-only \
  --tmpfs /tmp \
  --tmpfs /workspace \
  piengine "$@"