# Retired database execution

Historical reference only, excluded from every build and runtime target. kgedit
creates offline TTL/Cypher bundles; database import belongs to external tooling.
The retired implementation depends on historical APIs and is not a supported
standalone component.

The corresponding tests are preserved unchanged in `tests/` but intentionally
excluded from CMake. The active integration harness is
`tests/integration/verify_standalone.py` and uses only this repository's fixtures.
