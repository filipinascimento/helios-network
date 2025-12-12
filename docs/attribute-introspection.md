# Attribute Introspection Cheatsheet

This short guide covers the new helpers that let you check whether an attribute exists, list available attributes, and read their type metadata across nodes, edges, and the network.

## Checking for an attribute
- `hasNodeAttribute(name)`
- `hasEdgeAttribute(name, pure = false)`
  - Pass `pure: true` to ignore node-to-edge passthroughs and only consider native edge attributes.
- `hasNetworkAttribute(name)`
- `NodeSelector.hasAttribute(name)` and `EdgeSelector.hasAttribute(name)` mirror the network checks for the respective scopes.

Example:
```js
if (!net.hasEdgeAttribute('capacity')) {
  net.defineEdgeAttribute('capacity', AttributeType.Float);
}

const edges = net.createEdgeSelector([0, 1]).asProxy();
if (edges.hasAttribute('capacity')) {
  // Safe to read from edges.capacity here
}
```

## Listing available attributes
- `getNodeAttributeNames()` → `string[]`
- `getEdgeAttributeNames()` → `string[]`
- `getNetworkAttributeNames()` → `string[]`

Example:
```js
const nodeAttributes = net.getNodeAttributeNames(); // ['weight', 'degree']
const edgeAttributes = net.getEdgeAttributeNames(); // ['capacity', 'length']
```

## Reading attribute type metadata
- `getNodeAttributeInfo(name)`
- `getEdgeAttributeInfo(name)`
- `getNetworkAttributeInfo(name)`

Each returns either `null` (if the attribute is missing) or an object with:
- `type`: the `AttributeType` constant used when defining the attribute
- `dimension`: number of elements per entity (e.g., 3 for a vec3)
- `complex`: `true` when the attribute is backed by the complex store

Example:
```js
const info = net.getEdgeAttributeInfo('capacity');
if (info) {
  console.log(info.type);      // AttributeType.Float
  console.log(info.dimension); // 1
  console.log(info.complex);   // false for numeric types
}
```

## Tips
- These helpers require an active network instance; they call into the same metadata that backs attribute buffers.
- Edge passthroughs (node-to-edge copies) count as attributes; use `hasEdgeAttribute(name, true)` to check only native edge definitions.
- For iterating or projecting attributes across subsets/all ids, see the selector guide: `docs/selectors.md`.
