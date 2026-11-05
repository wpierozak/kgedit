# kgedit

Standalone C++23 tool for versioned RDF graphs. It validates checksummed XML
changelogs, composes complete Turtle, and creates deterministic TTL/Cypher import
bundles for Neo4j. It runs offline and does not handle database credentials.

The repository contains the tool, its tests, a small generic example, and an
optional FEE instance-generation library. Application datasets live in their own
repositories; building or testing kgedit does not require dcs-agent.

## Build and run

```sh
docker build --target runtime -t kgedit .
docker run --rm kgedit --help
docker run --rm --network none -v "$PWD/examples/changelog:/data:ro" kgedit validate /data/root.xml
```

The executable is `/build/kgedit` in the image. To build an artifact bundle, create
an output mount and specify the future Neo4j-visible TTL location:

```sh
mkdir -p artifacts
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -v "$PWD/examples/changelog:/data:ro" -v "$PWD/artifacts:/output" \
  kgedit build /data/root.xml /output/example --ttl-uri file:///import/combined.ttl
```

The new output directory contains `combined.ttl`, `import.cypher`, and
`manifest.xml`. Existing output paths are never overwritten. The manifest records
source changeset checksums, artifact checksums, counts, and required Neo4j/n10s
versions. Identical inputs and TTL URI produce identical artifacts.

For a native build with the dependencies available, use CMake 3.22+, a C++23
compiler, CLI11 **2.7.2**, and the pinned dependency toolchain shown in the Dockerfile:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix /your/install/prefix
```

## Commands

[CLI11](https://github.com/CLIUtils/CLI11/releases/tag/v2.7.2) handles argument
parsing. Use `kgedit --help` or `kgedit COMMAND --help` for generated usage and
required arguments. Exactly one subcommand is required. `build` requires
`--ttl-uri`; named options may appear before or after positional arguments.
Parsing and help complete before any changelog is opened or output is written.
The Docker build verifies the pinned CLI11 source archive's SHA-256 checksum.

```sh
kgedit --help
kgedit validate ROOT.xml
kgedit compose ROOT.xml OUTPUT.ttl
kgedit build ROOT.xml NEW_OUTPUT_DIR --ttl-uri URI
kgedit new-change ROOT.xml NEW_DIRECTORY CHANGESET_ID INPUT.ttl
kgedit append ROOT.xml NEW_DIRECTORY/changelog.xml
kgedit create-instance-config ROOT.xml OUTPUT.xml IRI_PREFIX
kgedit generate-instances CORE_ROOT.xml FEE.xml NEW_DIRECTORY CHANGESET_ID
```

Canonical IRIs identify records and relationship endpoints. Mandatory `g:recordId`
values retain existing application record IDs; UUID properties stay preserved.
A later definition of the same identity replaces the complete record. Unmentioned
records remain unchanged. Deletes and schema replacement are unsupported.
See [FORMAT.md](FORMAT.md) and [the example](examples/changelog/data.ttl).

## External Neo4j import

Use an empty database with Neo4j **5.26** and n10s **5.26.0**. The Dockerfile's
`neo4j` target installs the pinned, checksummed plugin. Enable `n10s.*` procedures.
The TTL URI must be accessible from Neo4j: n10s reads `file:///import/combined.ttl`
as `/import/combined.ttl` inside its container. It does not prepend an import path.

Verify the artifact checksums against the manifest, place the matching TTL at the
specified location, then execute the generated script externally:

```sh
cypher-shell --fail-fast -f artifacts/example/import.cypher
```

The script checks the empty target, creates schema/n10s configuration, imports
RDF, and projects application labels, native properties and direct relationships
in one transaction. Parallel relationships and self-loops remain distinct. It
cleans imported RDF resources after successful projection. Schema and RDF import
commit separately and may remain after failure; use a fresh disposable target to
retry. Reimporting into the populated graph is refused. Database execution,
credentials, incremental application and database ledgers are outside the tool.

## Verification and API documentation

```sh
docker build --target build -t kgedit-build:verification .
scripts/verify-integration.sh
mkdir -p build
doxygen Doxyfile
```

Docker builds run the unchanged C++ unit tests and CLI smoke tests. Integration
verification uses only bundled fixtures and an owned disposable Neo4j container:
canonical identities, manifests, native types, arrays, parallel edges, self-loops,
populated-target refusal, failed import/projection, and FEE instance generation.
No external dataset, Git history, or existing database is required.

Doxygen writes HTML/XML to `build/doxygen/`, includes private helpers and archived
APIs, and treats documentation warnings as errors. The source namespace and
internal library names remain `entity_rdf` to preserve the tested API and RDF
vocabulary. Retired database execution code is reference-only under
`archive/legacy_database/` and is excluded from all builds.
