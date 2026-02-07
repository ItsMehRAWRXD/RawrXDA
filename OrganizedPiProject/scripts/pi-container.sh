#!/bin/bash
# π-Engine Container Wrapper - Enterprise SaaS Ready

LANGUAGE="$1"
CODE="$2"

# Security: Resource limits + network isolation
docker run --rm -i \
  --memory=256m --cpus=0.5 \
  --network=none --read-only \
  --tmpfs=/tmp:noexec,nosuid,size=100m \
  --tmpfs=/workspace:exec,size=50m \
  --user=1000:1000 \
  pi-engine:latest \
  "$LANGUAGE" <<< "$CODE"