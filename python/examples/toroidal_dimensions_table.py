from __future__ import annotations

import argparse

from helios_network import DimensionMethod, Network


def strides_from_sides(sides: list[int]) -> list[int]:
    strides = [1] * len(sides)
    for d in range(1, len(sides)):
        strides[d] = strides[d - 1] * sides[d - 1]
    return strides


def coordinates_from_linear(index: int, sides: list[int]) -> list[int]:
    coords = [0] * len(sides)
    value = index
    for d, side in enumerate(sides):
        coords[d] = value % side
        value //= side
    return coords


def linear_from_coordinates(coords: list[int], strides: list[int]) -> int:
    return sum(coords[d] * strides[d] for d in range(len(coords)))


def create_toroidal_network(sides: list[int]) -> Network:
    total_nodes = 1
    for side in sides:
        total_nodes *= side
    network = Network(directed=False)
    node_ids = network.add_nodes(total_nodes)
    strides = strides_from_sides(sides)
    edges: list[tuple[int, int]] = []
    for idx in range(total_nodes):
        coords = coordinates_from_linear(idx, sides)
        for d in range(len(sides)):
            neighbor = coords.copy()
            neighbor[d] = (neighbor[d] + 1) % sides[d]
            n_idx = linear_from_coordinates(neighbor, strides)
            edges.append((node_ids[idx], node_ids[n_idx]))
    network.add_edges(edges)
    return network


def print_table(rows: list[list[str]]) -> None:
    widths = []
    for col in range(len(rows[0])):
        widths.append(max(len(row[col]) for row in rows))

    for i, row in enumerate(rows):
        line = " | ".join(row[col].ljust(widths[col]) for col in range(len(row)))
        print(line)
        if i == 0:
            print("-+-".join("-" * widths[col] for col in range(len(row))))


def main() -> None:
    parser = argparse.ArgumentParser(description="Print max-dimension table for 1D/2D/3D/4D toroidal regular networks.")
    parser.add_argument("--quick", action="store_true", help="Use smaller networks for faster runs.")
    parser.add_argument("--large", action="store_true", help="Use larger (20k-50k node) network sizes.")
    args = parser.parse_args()
    if args.quick and args.large:
        raise ValueError("Use either --quick or --large, not both")

    if args.quick:
        configs = [
            ("1D", [96], 6, 64),
            ("2D", [18, 18], 6, 64),
            ("3D", [8, 8, 8], 4, 48),
            ("4D", [6, 6, 6, 6], 3, 32),
        ]
    elif args.large:
        configs = [
            ("1D", [32768], 8, 64),
            ("2D", [224, 224], 8, 64),
            ("3D", [32, 32, 32], 6, 48),
            ("4D", [14, 14, 14, 14], 4, 32),
        ]
    else:
        configs = [
            ("1D", [128], 8, 192),
            ("2D", [24, 24], 8, 192),
            ("3D", [10, 10, 10], 6, 192),
            ("4D", [8, 8, 8, 8], 4, 192),
        ]

    rows = [["Dim", "Sides", "Nodes", "Edges", "max d_i(r)", "max D(r)"]]
    for label, sides, max_level, sample_cap in configs:
        net = create_toroidal_network(sides)
        local = net.measure_node_dimension(0, max_level=max_level, method=DimensionMethod.LeastSquares, order=2)
        sampled = list(range(min(sample_cap, net.node_count())))
        global_stats = net.measure_dimension(max_level=max_level, method=DimensionMethod.LeastSquares, order=2, nodes=sampled)
        rows.append(
            [
                label,
                "x".join(str(v) for v in sides),
                str(net.node_count()),
                str(net.edge_count()),
                f"{max(float(v) for v in local['dimension']):.3f}",
                f"{max(float(v) for v in global_stats['global_dimension']):.3f}",
            ]
        )

    print_table(rows)


if __name__ == "__main__":
    main()
