# kgedit development

- kgedit is an offline C++23 tool for versioning, validation, complete TTL/Cypher artifact creation without domain-specific instance generation. Do not add database connections or credentials to the CLI.
- Preserve existing application IRIs, UUIDs and record IDs. Mint missing canonical IRIs once during authoring; never during composition.
- Keep graph composition, DTOs, RDF/XML codecs, Cypher generation and artifact publication separate. FEE-specific generation belongs on `feature/fee-parameters`.
- Prefer single-responsibility classes and methods, smart pointers to interfaces for shared polymorphic dependencies, ranges and zero-copy access where practical. Avoid auto except for complex STL types.
- Document every C++ class, struct and method with Doxygen, including private helpers. Keep parameter, return-value and failure contracts accurate.
- Compile and run unit tests using the Dockerfile. Existing tests must remain unchanged unless the user approves edits; add new tests for new behavior.
- Test paths mirror production structure. Use GTest fixtures for nontrivial tests and named fixture helpers for setup and repeated assertions.
- Run clang-format before finishing C++ work. Doxygen must pass with warnings treated as errors.
- Run `scripts/verify-integration.sh` for artifact/import changes. It owns a disposable Neo4j+n10s instance and never accepts a production database endpoint.
- Dataset repositories own immutable published Turtle/XML changelogs and provenance. Do not bundle application datasets in the tool runtime.
- Ask the user when a design decision has materially different valid solutions.
- At the end ask for review. Commit only after acceptance, following Conventional Commits with a `(codex)` tag.
