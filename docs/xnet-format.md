# XNET Textual Format Specification

Helios persists human-readable network snapshots via the XNET container. Two
variants are recognized by the runtime:

- **XNET 1.0.0** – the current canonical grammar emitted by `saveXNet()` and the
  CLI tooling.
- **Legacy XNET** – the older dialect consumed by early Helios prototypes.

Both variants describe directed or undirected graphs with optional attributes for
vertices, edges, and the graph itself. This document specifies the wire format,
focusing on syntax and validation rules rather than API usage.

## Shared Conventions

- **Encoding** – Files are UTF-8 text. Line terminators may be `\n` or `\r\n`.
- **Whitespace** – Tokens are separated by ASCII whitespace. Leading and trailing
  whitespace on each line is ignored unless the line belongs to an attribute
  value block.
- **Comments** – Lines starting with `##` are treated as comments. Comments may
  appear between sections but are forbidden inside edge lists or attribute
  blocks. A lone `#` introduces a directive, not a comment.
- **Blank lines** – Ignored between sections, disallowed inside edge lists and
  attribute blocks.
- **Indices** – Node and edge identifiers are zero-based integers.
- **Strings** – Values may be quoted or unquoted. Quoted strings support the
  escapes `\n`, `\r`, `\t`, `\\`, and `\"`. Unquoted strings cannot start with `#`
  and end at the first trailing whitespace. Empty values must be written as
  `""` or nothing inside quotes.
- **Numbers** – Integers are base-10. `i`/`u` tokens store 32-bit signed or
  unsigned values, while `I`/`U` tokens store 64-bit integers (BigInteger).
  Floating-point numbers are parsed using `strtod` and accept scientific
  notation.
- **Attribute blocks** – Each attribute declaration consumes a fixed number of
  subsequent value lines: the vertex count for `#v`, the edge count for `#e`,
  and exactly one line for `#g`. Comments or blank lines inside a block are
  invalid.
- **Name uniqueness** – Attribute names must be unique per scope (vertex, edge,
  graph). Duplicate declarations are rejected.

## XNET 1.0.0 Grammar

```
file          := header vertices graph_attrs edges edge_list vertex_attrs original_ids edge_attrs
header        := "#XNET 1.0.0"
vertices      := "#vertices" SP count
graph_attrs   := { graph_attr }
graph_attr    := "#g" SP quoted_name SP type_token NEWLINE value_line
edges         := "#edges" SP ("directed" | "undirected")
edge_list     := { edge_line }
edge_line     := index SP index
vertex_attrs  := { vertex_attr_block }
vertex_attr_block := "#v" SP quoted_name SP type_token NEWLINE dict_block? value_line{count}
original_ids  := auto block written by Helios writer
edge_attrs    := { edge_attr_block }
edge_attr_block := "#e" SP quoted_name SP type_token NEWLINE dict_block? value_line{edge_count}
dict_block    := dict_header NEWLINE dict_entry{count}
dict_header   := ("#vdict" | "#edict" | "#gdict") SP quoted_name SP count
dict_entry    := index SP string
```

`SP` represents one or more whitespace characters. `value_line` is shaped by the
attribute type (see below) and must not be empty or commented.

### Directive Semantics

| Directive            | Purpose                                                    |
|----------------------|------------------------------------------------------------|
| `#XNET 1.0.0`        | Mandatory header identifying the format version.           |
| `#vertices <count>`  | Declares the number of active vertices. Must be ≥ 0.       |
| `#g "<name>" <type>` | Defines a graph-level attribute (optional, multiple).      |
| `#edges <mode>`      | Declares whether the graph is `directed` or `undirected`.  |
| `<from> <to>`        | Edge list entry using zero-based indices.                  |
| `#v "<name>" <type>` | Vertex attribute block (optional, multiple).               |
| `#e "<name>" <type>` | Edge attribute block (optional, multiple).                 |
| `#vdict "<name>" <count>` | Categorical dictionary for a vertex attribute.        |
| `#edict "<name>" <count>` | Categorical dictionary for an edge attribute.          |
| `#gdict "<name>" <count>` | Categorical dictionary for a graph attribute.          |

Helios always appends a vertex attribute named `_original_ids_` (type `s`)
containing the pre-compaction vertex identifiers. Readers must accept the block
when present but may also ingest files that omit it.

### Type Tokens

| Token | Meaning                           |
|-------|-----------------------------------|
| `s`   | UTF-8 string (scalar)             |
| `f`   | 32-bit float scalar               |
| `fN`  | Float vector of length `N ≥ 2`    |
| `i`   | 32-bit signed integer scalar      |
| `iN`  | Signed integer vector (`N ≥ 2`)   |
| `u`   | 32-bit unsigned integer scalar    |
| `uN`  | Unsigned integer vector (`N ≥ 2`) |
| `I`   | 64-bit signed BigInteger scalar   |
| `IN`  | BigInteger vector (`N ≥ 2`)       |
| `U`   | 64-bit unsigned BigInteger scalar |
| `UN`  | Unsigned BigInteger vector (`N ≥ 2`) |
| `c`   | Categorical scalar (signed 32-bit codes) |
| `cN`  | Categorical vector (`N ≥ 2`)            |

Strings cannot be vectorized. For numeric and categorical vectors, elements on each value line
are separated by single spaces. All attribute values must be present; missing or
extra tokens cause the parser to reject the file.

Categorical values are signed 32-bit integers. The recommended missing-value
sentinel is `-1`, leaving `0` as a valid category id.

### Categorical Dictionaries

Categorical attributes (`c`/`cN`) may declare a dictionary stanza immediately
after the attribute header. The dictionary uses explicit integer ids so sparse
category ids are supported. Each entry line contains the numeric id and its
string label. The attribute values remain numeric codes in the value block.
When no dictionary stanza is present, categorical values are treated as raw
codes without labels.

### Validation Rules

- The edge list must contain exactly the number of active edges the writer
  recorded (`edge_count`). Edges referencing a vertex ≥ `count` are invalid.
- Attribute blocks must match the declared dimensions and counts. Each line is
  parsed independently; trailing characters after the expected elements raise an
  error.
- Graph attributes are restricted to scalar vectors and may not contain nested
  arrays.

## Legacy XNET Grammar

Legacy files omit the version banner and are more permissive:

```
legacy_file    := vertices legacy_labels? legacy_edges? vertex_attrs edge_attrs
vertices       := "#vertices" SP count { legacy_token }
legacy_labels  := string_line{count}   // optional; aborted if next line starts with '#'
legacy_edges   := legacy_edges_header legacy_edge_list?
legacy_edges_header := "#edges" { legacy_token }
legacy_token   := "directed" | "undirected" | "weighted" | "nonweighted"
legacy_edge_list := { legacy_edge_line }
legacy_edge_line := index SP index [SP weight]
vertex_attrs   := { legacy_vertex_attr_block }
edge_attrs     := { legacy_edge_attr_block }
```

### Differences from 1.0.0

- The file may consist solely of vertices (no `#edges` section).
- Immediately after `#vertices` the writer may include `count` bare string lines.
  When present they are promoted to a vertex attribute named `Label`. The block
  ends when another directive begins; partial blocks are rejected.
- `#edges` accepts any ordering of the tokens listed above. If `weighted` is
  present, each edge line carries a third column parsed as a float. The loader
  converts the column into an edge attribute named `weight`.
- Graph attributes (`#g`) are not recognized. Encountering one produces an
  error.
- Type tokens are restricted to the historical set:

  | Token | Meaning                       |
  |-------|-------------------------------|
  | `s`   | String scalar                 |
  | `n`   | Float scalar                  |
  | `v2`  | 2D float vector               |
  | `v3`  | 3D float vector               |

- Comments are not allowed inside the edge list, matching the 1.0.0 behavior.
- Legacy categorical attributes can be encoded as string attributes whose names
  end with `__category`. When loading, Helios strips the suffix, converts the
  string values into categorical codes (sorted by descending frequency, with
  a bytewise label tie-break). Missing or empty strings are treated as
  `__NA__`; the sentinel string maps to id `-1` and is treated as missing.
  Other labels receive ids starting at `0`.

### Normalization

When Helios reads a legacy document it:

1. Reconstructs the graph using the declared vertex/edge counts.
2. Converts legacy label and weight blocks into regular attributes (`Label`,
   `weight`).
3. Compacts vertex identifiers and emits `_original_ids_` so subsequent saves use
   the canonical XNET 1.0.0 syntax.

Writers no longer emit the legacy dialect, but the reader remains backward
compatible and performs the conversions above automatically.

## Error Handling

The native loader is strict and aborts on:

- Unknown directives or tokens.
- Duplicate attribute names per scope.
- Malformed type tokens or vector dimensions `< 1`.
- Attribute blocks with missing/extraneous value lines.
- Comments or blank lines inside edge lists or attribute blocks.
- Integer or float values that fail to parse in their expected range.

Implementations targeting other platforms should enforce the same rules to
guarantee interoperability with existing Helios builds.
