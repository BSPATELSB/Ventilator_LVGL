#!/usr/bin/env bash
# Wrapper script for project setup
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/project_setup.sh" "$@"
