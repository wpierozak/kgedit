#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
docker build --target build -t kgedit-build:verification .
docker build --target neo4j -t kgedit-neo4j:verification .
python3 tests/integration/verify_standalone.py
