#!/bin/bash
# AI Client Wrapper for Unix/Linux/macOS
# This script provides a consistent interface for calling AI clients from the bridge server

set -e

# Check if we have arguments
if [ $# -eq 0 ]; then
    echo "Usage: $0 [java|php] [arguments...]"
    exit 1
fi

CLIENT_TYPE="$1"
shift

# Check if API key is set
if [ -z "$GEMINI_API_KEY" ]; then
    echo "Error: GEMINI_API_KEY environment variable is not set"
    exit 1
fi

case "$CLIENT_TYPE" in
    "java")
        # Use Java AI client
        java -cp .:jackson-core-2.15.2.jar:jackson-databind-2.15.2.jar:jackson-annotations-2.15.2.jar AIChatClient "$@"
        ;;
    "php")
        # Use PHP AI client
        php ai_cli.php "$@"
        ;;
    *)
        echo "Error: Unknown client type '$CLIENT_TYPE'. Use 'java' or 'php'."
        exit 1
        ;;
esac
