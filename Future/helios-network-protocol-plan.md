# Helios Network Protocol + Journal Plan

Date: 2026-02-02
Owner: TBD
Scope: helios-network-v2 (C core + WASM + JS + Python)

## Goal
Design and implement a compact protocol that supports streaming commands, queries, and journaled changes for Helios Network. Provide both a binary wire format and a compact human-readable text format. Expose the protocol in C, JavaScript, and Python, with strong support for fine-grained attribute updates and journal replay.

## Non-negotiables (WASM + Performance)
- Avoid caching TypedArray views across allocation-prone calls.
- Allocate first, view second.
- Keep large buffers in WASM; avoid JS-side copies.

## Use Cases
- Remote execution (HTTP, WebSocket, pipes): send command/query batches.
- Streaming sync: send journal entries incrementally.
- Storage: persist an append-only log for replay.
- Compact debugging: text encoding for review and troubleshooting.

## Design Principles
- Binary and text formats are isomorphic (same op set, semantics).
- Streaming-friendly framing (record boundaries, length-prefixed).
- Versioned schema with forward compatibility.
- Deterministic replay for commands; queries do not mutate.
- Optional compression at batch level.

## Protocol Model
### Envelope
Fields (binary and text):
- magic, version
- flags (compression, query/command mix allowed)
- graph_id (string or uint64)
- sequence_start, sequence_end (optional)

### Record
Each record contains:
- op_id (uint8 or varint)
- op_flags (is_query, has_response)
- correlation_id (for request/response mapping)
- payload_length
- payload

### Responses
- A response record echoes correlation_id.
- Queries return data payloads.
- Commands return ok/error and any created ids (e.g., add_nodes returns allocated ids).
- Batch/stream errors include the failing record index and byte offset so callers know where decoding stopped.
- Command responses can be captured as variables in the text format and via result slots in binary.

## Operation Set (MVP)
### Commands (mutations)
- add_nodes(count)
- remove_nodes(ids)
- add_edges(pairs)
- remove_edges(ids | pairs)
- set_directed(bool)

### Attributes
- set_attr_meta(scope, name, dtype, default, track)
- set_attr_values_sparse(scope, attr, ids[], values[])
- set_attr_values_const(scope, attr, ids[], value)
- set_attr_values_selector(scope, attr, selector, values_or_const)
- clear_attr_values(scope, attr, ids[] | range)
- remove_attr(scope, name)
- rename_attr(scope, old, new)

### Queries (read-only)
- get_node_count
- get_edge_count
- get_neighbors(node_id, mode)
- get_edges_for_nodes(ids)
- list_attrs(scope)
- get_attr_values_sparse(scope, attr, ids[])
- get_graph_info
- select_nodes(where_expr)
- select_edges(where_expr)

### Journal / Sync
- checkpoint (optional: includes snapshot payload)
- compact (server-side only)

## Attribute Update APIs (C Core)
Add C APIs that support fine-grained updates without exposing raw buffers:
- hn_attr_set_sparse_nodes(attr_id, node_ids, values, count)
- hn_attr_set_sparse_edges(attr_id, edge_ids, values, count)
- hn_attr_set_const_nodes(attr_id, node_ids, count, value)
- hn_attr_set_const_edges(attr_id, edge_ids, count, value)
- hn_attr_set_selector_nodes(attr_id, selector, values_or_const)
- hn_attr_set_selector_edges(attr_id, selector, values_or_const)

Support journal control:
- hn_attr_track(attr_id, bool)
- hn_attr_touch_range(attr_id, start, count)

## Text Format (Compact, Human Readable)
Example:
HNP/1 graph=demo dir=0
newIDs = ADD_NODES n=4
ADD_EDGES pairs=[(0,1),(1,2),(2,3)]
SET_ATTR_META scope=node name=weight type=f32 default=1.0 track=1
SET_ATTR_VALUES scope=node name=weight ids=[0,2] values=[0.5,2.0]
GET_NEIGHBORS node=2 mode=out -> cid=7
RSP cid=7 neighbors=[1,3]

### Relative Ids / Result References
- add_nodes/add_edges return ids; text format can bind them to a variable name.
- Relative id mode remaps indices against a prior result set (no mixing with absolute ids).

Examples:
newIDs = ADD_NODES n=4
ADD_EDGES pairs=[(0,1),(1,2),(2,3)] ! relative newIDs
SET_ATTR_VALUES scope=node name=weight ids=[0,2] values=[0.5,2.0] ! relative newIDs

Default behavior (optional):
- If no variable is provided, the runtime may store results in a reserved name
  like added_node_ids / added_edge_ids (overwritten each time).

## Binary Format (Compact Wire)
- Magic: HNP\x01
- All arrays length-prefixed
- Little-endian
- Record layout:
  - u8 op_id
  - u8 flags
  - u16 reserved
  - u32 correlation_id
  - u32 payload_len
  - u8 payload[payload_len]

## Compression
- No compression in MVP; allow transport-level compression.
- Reserve envelope flags for future compression support.

## C/WASM Integration
- Implement decode/encode in C.
- Provide entry points:
  - hn_apply_batch(ptr, len)
  - hn_emit_batch(ptr, max_len, options)
- Ensure allocation-prone work done before taking views.

## JS API
- network.applyBatch(binary_or_text)
- network.encodeBatch(ops, { format })
- network.decodeBatch(data)
- network.journal({ since, format })
- network.selectNodes(whereExpr, { format })
- network.selectEdges(whereExpr, { format })

## Python API
- Network.apply_batch(data)
- Network.encode_batch(ops, format)
- Network.decode_batch(data)
- Network.journal(since, format)
- Network.select_nodes(where_expr, format=None)
- Network.select_edges(where_expr, format=None)

## Testing Plan
- Round-trip encode/decode parity (binary <-> text).
- Deterministic replay of command sequences.
- Query responses match direct API calls.
- Attribute updates: sparse, const, selector.
- Journal: apply -> snapshot -> replay -> match.

## Documentation
- Add protocol spec in docs/.
- Provide examples for:
  - remote query
  - remote mutation
  - streaming journal
  - snapshot + replay

## Delivery Phases
Phase 1: Spec + MVP
- Define op list, schemas, and versioning.
- Implement minimal encode/decode in C for core ops.
- JS/Python wrappers for apply_batch.

Phase 2: Attribute APIs
- Implement fine-grained attr setters in C.
- Expose to JS/Python.
- Add journal track/touch support.

Phase 3: Journaling + Streaming
- Append-only log support.
- Snapshot/checkpoint.
- Streaming reader/writer utilities.
 - Structured error reporting (record index + byte offset).

Phase 4: Optimization + Compression
- Compression options.
- Batch size tuning.
- Partial decode for streaming.

## Open Questions
- Preferred compression library (zstd vs lz4) for a future phase?
- Selector / filter language definition (nodes + edges).
- Graph id and authorization model for remote execution.
- Exact dtype support list for attributes in protocol.
- Error schema for responses.

## Result References (Binary)
- Each record can request a result slot id (u32) for returned ids.
- Subsequent records may declare id operands as relative to a slot id.
- Relative id usage must be explicit; mixing relative and absolute ids is invalid.

## Attribute Query Language (Initial)
Target: simple expressions with AND/OR/NOT + parentheses, with comparisons against attribute values.

### Supported Types (Phase 1)
- numeric (float, int)
- categorical
- strings
Note: vectorized/multi-dimensional attributes are excluded from query evaluation for now.

### Node Query Examples
- weight < 0 AND label != \"foo\"
- NOT (group = \"A\" OR rank >= 3)
- $any.neighbor.score > 2
- $both.neighbor.type = \"hub\"

### Edge Query Examples
- weight < 0 AND $src.score > 0
- $dst.type = \"target\" AND $any.type = \"source\"
- $both.score >= 1.5

### Endpoint/Neighbor Modifiers
- $src.attrName, $dst.attrName
- $any.attrName, $both.attrName
- $any.neighbor.attrName, $both.neighbor.attrName

### Semantics
- $any: predicate is true if any endpoint (or any neighbor) matches.
- $both: predicate is true only if both endpoints (or all neighbors) match.
