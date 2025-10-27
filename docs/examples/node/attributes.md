# Attributes Showcase (Node.js)

This guide explains the `docs/examples/node/attributes.mjs` script, which exercises every attribute type supported by Helios Network across node, edge, and network scopes.

---

## Prerequisites

Ensure the workspace is prepared:

```bash
npm install
npm run build
npm run build:wasm
```

These commands build the WASM core and ESM bundle used by the example.

---

## Running the script

Execute the example from the repository root:

```bash
node docs/examples/node/attributes.mjs
```

The script prints sections for setup, node attributes, edge attributes, network attributes, and teardown.

---

## What the script demonstrates

### Node attributes

Defined attributes:

| Name       | Type                          | Dimension | Notes                                    |
|------------|-------------------------------|-----------|------------------------------------------|
| `label`    | `AttributeType.String`        | 1         | Uses `setNodeStringAttribute` and `getNodeStringAttribute`. |
| `active`   | `AttributeType.Boolean`       | 1         | Backed by a `Uint8Array` view.           |
| `position` | `AttributeType.Float`         | 3         | Demonstrates multi-dimensional buffers (`Float32Array`). |
| `rank`     | `AttributeType.Integer`       | 1         | Uses `BigInt64Array` for integer values. |
| `metadata` | `AttributeType.Javascript`    | 1         | Stores arbitrary JS objects via `set` / `get` / `delete`. |

Operations covered:

- Defining attributes with `defineNodeAttribute`.
- Populating numeric views and multi-dimensional entries.
- Storing and clearing string attributes via the helper methods.
- Managing JavaScript-backed values (including `delete` to clear metadata).

### Edge attributes

Defined attributes:

| Name       | Type                           | Dimension | Notes                                          |
|------------|--------------------------------|-----------|------------------------------------------------|
| `weight`   | `AttributeType.Double`         | 1         | Stored in a `Float64Array`.                    |
| `traffic`  | `AttributeType.UnsignedInteger`| 1         | Uses `BigUint64Array`.                         |
| `kind`     | `AttributeType.Category`       | 1         | Demonstrates categorical integer values.       |
| `payload`  | `AttributeType.Data`           | 1         | JS-managed handles suitable for binary blobs.  |
| `marker`   | `AttributeType.String`         | 1         | Managed via `setEdgeStringAttribute`.         |

Operations covered:

- Writing scalar values to typed array views.
- Handling BigInt-compatible numeric buffers.
- Storing arbitrary payloads (e.g., Node.js `Buffer` objects) with `.set`.
- Clearing values with `.delete` and string helpers.

### Network attributes

Defined attributes:

| Name       | Type                           | Dimension | Notes                                             |
|------------|--------------------------------|-----------|---------------------------------------------------|
| `title`    | `AttributeType.String`         | 1         | Demonstrates graph-level string metadata.         |
| `counters` | `AttributeType.UnsignedInteger`| 2         | Shows multi-dimensional unsigned BigInt buffers.  |
| `flags`    | `AttributeType.Boolean`        | 4         | Writes boolean flags into a `Uint8Array`.         |
| `settings` | `AttributeType.Javascript`     | 1         | Stores configuration objects.                     |

Operations covered:

- Declaring network attributes with `defineNetworkAttribute`.
- Writing multi-slot data (e.g., 2-element counter array, 4 boolean flags).
- Managing JS-backed settings with `.set` and `.delete`.
- Clearing string values via `setNetworkStringAttribute(name, null)`.

---

## Key takeaways

- **Typed-array views:** For numeric attributes, `get*AttributeBuffer` returns an object with a `.view` property referencing the WASM heap. Indexing is direct (`view[nodeId]`, `view[edgeId]`) and multi-dimensional attributes expand sequentially (`dimension` elements per entity).
- **String attributes:** Use the dedicated `set*StringAttribute`/`get*StringAttribute` helpers. Passing `null` (or `undefined`) clears the slot.
- **JavaScript/Data attributes:** Buffers expose `.set(index, value)`, `.get(index)`, and `.delete(index)`; internally, integer handles are stored in WASM while the values live in a `Map`.
- **Dimension awareness:** Each buffer wrapper exposes a `dimension` field so you can compute offsets (`index * dimension`) for multi-element attributes.

Use this script as a template when designing your own attribute layouts or crafting integration tests.
