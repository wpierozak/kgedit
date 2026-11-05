# RDF records and identity

The format is a lossless RDF representation of the application property graph,
with explicit relationship resources. It is not a conversion of the application
into n10s's default `Resource` model, nor an OWL reasoner or a general RDF importer.
rdf4cpp validates Turtle syntax and known literal datatypes; the graph mapper
validates the supported vocabulary, ownership, identities and endpoints. No SHACL
inference or ontology consistency reasoning is claimed.

## Record vocabulary

All record subjects are explicit absolute IRIs. Blank nodes and language-tagged
literals are rejected: neither has a reversible mapping to the supported Neo4j
property model. Standard RDF prefixes are allowed in authored files.

| Meaning | RDF spelling |
| --- | --- |
| Node subject | Existing canonical application IRI; mint `urn:alice:dcs:node:` + encoded record ID only when missing |
| Relationship subject | Existing canonical relationship IRI; otherwise `urn:alice:dcs:relationship:` + encoded record ID |
| Preserved record ID | One `urn:dcs:graph:recordId` string literal on each canonical record |
| Node class | `rdf:type <urn:dcs:graph:Node>` |
| Relationship class | `rdf:type <urn:dcs:graph:Relationship>` |
| Application labels | repeated `urn:dcs:graph:label` string values |
| Relationship type | one `urn:dcs:graph:relationshipType` string |
| Direction | `urn:dcs:graph:source` and `urn:dcs:graph:target` node IRIs |
| Property name | predicate `urn:dcs:property:` + encoded exact property name |

Percent encoding leaves only ASCII letters, digits and `-._~` unescaped and uses
uppercase hex. The decoder rejects alternate spellings to prevent ID aliases.
Each subject has one record type. A property has one value; repeated distinct
values are rejected. Identical RDF triples retain RDF set semantics. A canonical IRI and its preserved record ID
must occur in only one file within a changeset; it may reappear in a later
changeset to replace its complete definition. Reassigning a canonical IRI to another record ID, or changing the IRI of an existing ID, is rejected. There are no endpoint declarations
or `reference_only` markers. A relationship refers directly to the existing node
IRI; references can cross input files.

```turtle
<urn:alice:dcs:relationship:link-1> a <urn:dcs:graph:Relationship> ;
    <urn:dcs:graph:recordId> "link-1" ;
    <urn:dcs:graph:source> <urn:alice:dcs:node:board-1> ;
    <urn:dcs:graph:target> <urn:alice:dcs:node:parameter-1> ;
    <urn:dcs:graph:relationshipType> "has_parameter" ;
    <urn:dcs:property:rank> "42"^^<http://www.w3.org/2001/XMLSchema#long> .
```

Another relationship resource with the same endpoints and type remains a separate
relationship, even when every property is identical. Self-loops are supported.
Existing IDs never depend on file order, a newly computed name, or Neo4j's
`elementId()`. File organization is chosen by the dataset owner; changeset XML records checksummed membership. Composition and serialization use ordered maps.

## Property values

Native scalars use `xsd:string`, `xsd:boolean`, `xsd:long` and `xsd:double`.
`xsd:integer` inputs are accepted only within signed 64-bit Neo4j range. Integers
and doubles retain distinct types; strings resembling numbers remain strings.
Double literals include `NaN`, `INF` and `-INF`. Null is not a stored property;
omit a property from the replacement record to remove it.

Arrays use an owned resource, retaining order, duplicates and empty arrays:

```turtle
<urn:alice:dcs:node:board-1> <urn:dcs:property:ports> <urn:dcs:value:board-ports> .
<urn:dcs:value:board-ports> a <urn:dcs:graph:Array> ;
    <urn:dcs:graph:length> "3"^^<http://www.w3.org/2001/XMLSchema#long> ;
    <urn:dcs:item:0> "B" ;
    <urn:dcs:item:1> "A" ;
    <urn:dcs:item:2> "B" .
```

Each value resource has exactly one owner and every item index exists once, with
no extra fields. Arrays must be homogeneous and flat. Generated value resource
IDs are SHA-256 of `record-subject + LF + exact-property-name`; replacements own
their complete value definition. Unowned or shared compound resources are rejected.
These resources are encoding details, not application nodes.

Neo4j types without an exact native RDF counterpart use a typed literal with
datatype `urn:dcs:neo4j:<kind>` and a reversible JSON lexical form. JSON is a
**literal encoding**, not a configuration-file format. This retains the historical
`neo4j-json-v1` codec's precision and components:

| Kind | Literal payload |
| --- | --- |
| `Date` | `{"@type":"Date","value":"2026-01-02"}` |
| `Time` | exact ISO time including nanoseconds and optional offset |
| `DateTime` | exact ISO datetime, with optional `timezone` IANA zone name |
| `Duration` | `{"@type":"Duration","value":[months,days,seconds,nanoseconds]}` |
| `point` | `{"@type":"point","srid":4326,"coordinates":[x,y]}`; supported SRIDs 7203, 9157, 4326, 4979 |
| `bytes`, `bytearray` | lowercase even-length hex in `value` |

The JSON `@type` must equal the datatype suffix. Typed arrays use the same indexed
resources with typed literals as items. Date/time strings, duration structure,
point dimensions and byte encodings are validated. Projection explicitly creates
Neo4j temporal/spatial values; comparison checks actual Neo4j types and values,
including temporal nanoseconds and time zones.

**Current projection limit:** native byte arrays have no supported core Cypher
constructor in the pinned Neo4j version. The RDF codec preserves them, but bundle
generation rejects them before publishing any output. Other supported tagged
values are emitted using native temporal/spatial Cypher constructors. Deletes,
schema removal/replacement, RDF-star, named graphs, RDF lists and arbitrary custom
RDF datatypes remain unsupported.

## Offline artifact boundary

The tool validates and composes files and emits complete TTL/Cypher bundles. It
never connects to Neo4j. External `cypher-shell --fail-fast` execution imports RDF
through n10s and projects the application graph in an empty database. Schema and
RDF import commit before transactional projection; failures can leave staging
resources. There is no database ledger, incremental application, adoption, or
live export/comparison. The manifest identifies source versions and artifact
checksums. Changing the file referenced by the import URI invalidates the bundle;
verify its checksum externally before execution.

Legacy `urn:dcs:node:` / `urn:dcs:relationship:` records without `g:recordId`
remain readable for offline compatibility and unchanged test fixtures. Their
subjects encode record IDs as before. New authored canonical records require
explicit record IDs; composition never invents missing canonical identities.
Legacy ID-only DTO serialization remains available, but `build` requires all
records to have canonical IRIs.

## Application identities and provenance

Four identifiers remain separate:

| Identifier | Contract |
| --- | --- |
| `iri` | Canonical RDF subject and application identity. Existing values remain exact; a supplied property must match the subject. Stored on nodes and relationships after projection. |
| `entity_identity` | Existing application UUID, preserved including duplicates and absence. |
| Node record ID | Preserved application record ID carried by `g:recordId`; stored as `_dataset_id` during application. |
| Relationship record ID | Preserved application record ID carried by `g:recordId`; stored on the relationship as `_dataset_id`. |

Existing IRIs, UUIDs and record IDs are preserved. New manually authored record
IDs must be nonempty and unique within their node or relationship namespace.
Canonical IRIs must be unambiguous across both namespaces. Dataset-specific
provenance and migration utilities belong in the dataset repository.

## Optional FEE instance extension

The separate `entity_rdf_instances` library retains UUID5 with the OID namespace
`6ba7b812-9dad-11d1-80b4-00c04fd430c8` over
`detector.upper() + '.' + board_name + '.' + parameter_name`. Only ASCII detector
case is normalized. Board and parameter names are kept exact. Instance IRIs append
percent-encoded local board and parameter names to the configured prefix. Local
extraction tries `#`, `/`, `:` in that order without percent decoding.

Generated node and relationship record IDs retain the historical SHA-256 JSON
identity formulas. Existing instance and relationship IDs are reused when the
stored identity and topology agree. See
[FeeInstanceBuilder](instances/src/FeeInstanceBuilder.cxx) and its unchanged UUID
fixtures. Generation has no database dependency.
