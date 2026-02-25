#!/usr/bin/env bash

set -euo pipefail

BASE_URL="${1:-https://example-tunnel.ngrok-free.app}"
API_KEY="${API_KEY:-MYSECRET}"
MODEL="${MODEL:-deepseek-coder-v2-32k}"

echo ">>> GET ${BASE_URL}/v1/models"
curl --silent --show-error --fail \
  -H "Authorization: Bearer ${API_KEY}" \
  "${BASE_URL}/v1/models" | jq .

echo
echo ">>> POST ${BASE_URL}/v1/chat/completions"
curl --silent --show-error --fail \
  -H "Authorization: Bearer ${API_KEY}" \
  -H "Content-Type: application/json" \
  -d @<(cat <<JSON
{
  "model": "${MODEL}",
  "temperature": 0.1,
  "messages": [
    {"role": "system", "content": "You are a helpful coding assistant."},
    {"role": "user", "content": "Return a Python function that adds two numbers."}
  ]
}
JSON
) \
  "${BASE_URL}/v1/chat/completions" | jq '.choices[0].message.content'

echo
echo "✅ Smoke test complete."

