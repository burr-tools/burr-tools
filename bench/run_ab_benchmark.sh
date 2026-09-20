#!/usr/bin/env bash
# run_ab_benchmark.sh — Alias for run_suite.sh
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/run_suite.sh" "$@"
