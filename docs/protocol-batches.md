# Protocol Batches (MVP)

This document describes the MVP batch formats supported by `applyTextBatch(...)` and `applyBinaryBatch(...)` on the JS wrapper.

## Text Batch (MVP)

Supported commands:

- `ADD_NODES n=<count>`
- `ADD_EDGES pairs=[(a,b),...]`
- `SET_ATTR_VALUES scope=node|edge name=<attr> ids=[...] values=[...]`

Relative ids:

- Use `! relative [varName]` to remap ids against a previously returned result set.
- Result sets are captured as `varName = ADD_NODES ...` or `varName = ADD_EDGES ...`.
- Defaults: `added_node_ids`, `added_edge_ids`.

Example:

```
newIDs = ADD_NODES n=4
ADD_EDGES pairs=[(0,1),(1,2),(2,3)] ! relative newIDs
SET_ATTR_VALUES scope=node name=weight ids=[0,2] values=[0.5,2.0] ! relative newIDs
SET_ATTR_VALUES scope=node name=label ids=[1,3] values=["a","b"] ! relative newIDs
```

Notes:
- String attributes accept quoted string values.
- Categorical attributes accept numeric ids or string labels (labels must exist in the category dictionary).

## Binary Batch (MVP)

### Header

- `u8[4]` magic = `HNPB`
- `u8` version = `1`
- `u8` flags
- `u16` reserved
- `u32` recordCount

### Record

- `u8` op (1=ADD_NODES, 2=ADD_EDGES, 3=SET_ATTR_VALUES)
- `u8` flags (bit0 = relative)
- `u16` reserved
- `u32` resultSlot (0 = unused)
- `u32` payloadLen
- `u8[payloadLen]` payload

### Payloads

**ADD_NODES**

- `u32` count

**ADD_EDGES**

- `u32` pairCount
- `u32` baseSlot
- `u32[pairCount*2]` pairs

**SET_ATTR_VALUES**

- `u8` scope (0=node, 1=edge)
- `u8` valueType (0=f64, 1=utf8 string)
- `u16` reserved
- `u32` idCount
- `u32` valueCount
- `u32` baseSlot
- `u32` nameLen
- `u8[nameLen]` name
- `u32[idCount]` ids
- valueType 0: `f64[valueCount]` values
- valueType 1: repeat `valueCount` times: `u32 len`, `u8[len]` bytes

Notes:
- `valueCount` can be 1 (broadcast) or match `idCount`.
- When `relative` is set, ids/pairs are treated as indices into `baseSlot`.
- String attributes require valueType 1.
- Categorical attributes accept either numeric ids (valueType 0) or string labels (valueType 1).
