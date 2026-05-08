#!/bin/bash

# Start Vellum Frontend (React Development Server)
# Run this after building the backend

PROXY_COMMAND=""
PROXY_CONFIG=""

if [[ "$1" == "--proxychains" ]]; then
    shift
    if [[ "$1" != "" && "$1" != --* ]]; then
        PROXY_CONFIG="$1"
        shift
    fi
    if command -v proxychains4 >/dev/null 2>&1; then
        PROXY_COMMAND="proxychains4"
    elif command -v proxychains >/dev/null 2>&1; then
        PROXY_COMMAND="proxychains"
    else
        echo "Error: proxychains is not installed. Install proxychains4 or proxychains to use this feature."
        exit 1
    fi
fi

run_with_proxy() {
    if [[ -n "$PROXY_COMMAND" ]]; then
        if [[ -n "$PROXY_CONFIG" ]]; then
            "$PROXY_COMMAND" -f "$PROXY_CONFIG" "$@"
        else
            "$PROXY_COMMAND" "$@"
        fi
    else
        "$@"
    fi
}

cd vellum/frontend

echo "Starting Vellum Frontend on http://localhost:3000"
echo "Make sure the backend is running on port 8080"
echo ""

run_with_proxy npm start