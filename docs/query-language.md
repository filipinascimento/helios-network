# Helios Query Language (MVP)

This document describes the MVP query language used by `selectNodes(...)` and `selectEdges(...)`.

## Overview

Queries are boolean expressions composed of predicates, logical operators, and parentheses.

- Logical operators: `AND`, `OR`, `NOT`
- Comparison operators: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Set membership: `IN (...)`
- Regex match: `=~ "pattern"`

## Grammar (Informal)

```
expr        := or_expr
or_expr     := and_expr (OR and_expr)*
and_expr    := not_expr (AND not_expr)*
not_expr    := NOT not_expr | primary
primary     := '(' expr ')' | predicate
predicate   := lhs op rhs
lhs         := [qualifier '.'] attribute
qualifier   := $src | $dst | $any | $both | $any.neighbor | $both.neighbor
op          := == | != | < | <= | > | >= | IN | =~
rhs         := number | string | '(' literal (',' literal)* ')'
literal     := number | string
```

## Scopes

- **Node queries** support:
  - direct attributes: `score > 0`
  - neighbor qualifiers: `$any.neighbor.attr`, `$both.neighbor.attr`
- **Edge queries** support:
  - edge attributes: `weight > 0`
  - endpoint qualifiers: `$src.attr`, `$dst.attr`, `$any.attr`, `$both.attr`

## Type Support

- Numeric attributes: all comparisons + `IN`.
- String attributes: `==`, `!=`, `IN`, regex `=~`.
- Categorical attributes:
  - `==` / `!=` with string labels
  - `IN` with string labels
- Vector attributes (dimension > 1): supported via **any-component** matching.

## Examples

### Node queries

```
score > 1.0 AND group != "A"
NOT (rank < 2 OR rank > 10)
$any.neighbor.type == "hub"
$both.neighbor.score >= 0
label IN ("foo", "bar", "baz")
name =~ "^node_[0-9]+$"
vec2[1] > 0.5
vec2.max >= 1.0
vec2.avg < 0.25
vec2.std > 0.2
vec2.abs > 1.0
vec2.dot(vec2b) > 0.5
vec2.any > 0.5
vec2.all > 0.1
vec2.dot([1, 0]) > 0.5
```

### Edge queries

```
weight > 0.5 AND $src.score > 1.0
$dst.type == "target"
$any.group IN ("A", "B")
$both.score >= 1.5
```

## Notes

- `IN (...)` lists cannot mix numeric and string values.
- Regex uses POSIX extended syntax.
- For performance, prefer numeric comparisons over regex.
- For vector attributes, a predicate is true if **any component** satisfies it unless you use an accessor/index.
- `.any` and `.all` force any/all component semantics explicitly.
- Accessors (`.min`, `.max`, `.avg`, `.median`, `.std`, `.abs`, `.dot(...)`) require numeric vectors.
- `dot(...)` accepts another attribute name or a vector literal: `dot([1, 0, 0])`.
