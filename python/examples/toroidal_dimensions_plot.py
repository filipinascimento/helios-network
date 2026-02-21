from __future__ import annotations

import argparse
from pathlib import Path

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


def format_table(rows: list[dict]) -> str:
    headers = ["Dim", "Sides", "Nodes", "Edges", "Max local d_i(r)", "Max global D(r)"]
    body = [
        [
            row["label"],
            "x".join(str(v) for v in row["sides"]),
            str(row["node_count"]),
            str(row["edge_count"]),
            f"{row['max_local']:.3f}",
            f"{row['max_global']:.3f}",
        ]
        for row in rows
    ]
    widths = []
    for col, header in enumerate(headers):
        w = len(header)
        for line in body:
            w = max(w, len(line[col]))
        widths.append(w)

    def pad(values: list[str]) -> str:
        return " | ".join(values[i].ljust(widths[i]) for i in range(len(values)))

    divider = "-+-".join("-" * w for w in widths)
    output = [pad(headers), divider]
    output.extend(pad(line) for line in body)
    return "\n".join(output)


def parse_method(value: str) -> DimensionMethod:
    normalized = value.strip().lower()
    mapping = {
        "fw": DimensionMethod.Forward,
        "forward": DimensionMethod.Forward,
        "bk": DimensionMethod.Backward,
        "backward": DimensionMethod.Backward,
        "ce": DimensionMethod.Central,
        "central": DimensionMethod.Central,
        "centered": DimensionMethod.Central,
        "ls": DimensionMethod.LeastSquares,
        "leastsquares": DimensionMethod.LeastSquares,
    }
    if normalized not in mapping:
        raise ValueError(f"Unknown method: {value}")
    return mapping[normalized]


def measure_configs(configs: list[dict], sample_nodes: int, method: DimensionMethod, order: int) -> list[dict]:
    rows = []
    for config in configs:
        network = create_toroidal_network(config["sides"])
        node_count = network.node_count()
        edge_count = network.edge_count()

        local = network.measure_node_dimension(
            0,
            max_level=config["max_level"],
            method=method,
            order=order,
        )

        sample_cap = int(config.get("sample_nodes", sample_nodes))
        sampled = list(range(min(max(1, sample_cap), node_count)))
        global_stats = network.measure_dimension(
            max_level=config["max_level"],
            method=method,
            order=order,
            nodes=sampled,
        )

        rows.append(
            {
                "label": config["label"],
                "sides": config["sides"],
                "node_count": node_count,
                "edge_count": edge_count,
                "r": list(range(config["max_level"] + 1)),
                "local_curve": [float(v) for v in local["dimension"]],
                "max_local": max(float(v) for v in local["dimension"]),
                "max_global": max(float(v) for v in global_stats["global_dimension"]),
            }
        )
    return rows


def plot_curves(rows: list[dict], output_path: Path, show: bool) -> None:
    try:
        import matplotlib.pyplot as plt
    except Exception as exc:  # pragma: no cover - optional runtime dependency
        raise RuntimeError(
            "matplotlib is required for plotting. Install with `python -m pip install matplotlib` "
            "or run with --skip-plot."
        ) from exc

    plt.figure(figsize=(9, 5.5))
    for row in rows:
        plt.plot(row["r"], row["local_curve"], marker="o", linewidth=1.8, label=f"{row['label']} ({'x'.join(map(str, row['sides']))})")
    plt.title("Local Dimension d_i(r) on Toroidal Regular Networks (node i=0)")
    plt.xlabel("Concentric level r")
    plt.ylabel("Local dimension d_i(r)")
    plt.grid(True, alpha=0.25)
    plt.legend()
    plt.tight_layout()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(output_path, dpi=180)
    print(f"\nSaved plot: {output_path}")
    if show:
        plt.show()
    plt.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Measure toroidal 1D/2D/3D/4D dimensions and plot local curves.")
    parser.add_argument("--quick", action="store_true", help="Use smaller networks for faster runs.")
    parser.add_argument("--large", action="store_true", help="Use larger (20k-50k node) network sizes.")
    parser.add_argument("--sample-nodes", type=int, default=192, help="Number of nodes sampled for global D(r).")
    parser.add_argument("--method", type=str, default="ls", help="Difference method: fw/bk/ce/ls.")
    parser.add_argument("--order", type=int, default=2, help="Estimator order.")
    parser.add_argument("--skip-plot", action="store_true", help="Only print table, do not generate plot.")
    parser.add_argument("--show", action="store_true", help="Display matplotlib window after saving.")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parent / "toroidal_local_dimension_curves.png",
        help="Output path for the plot image.",
    )
    args = parser.parse_args()
    if args.quick and args.large:
        raise ValueError("Use either --quick or --large, not both")

    method = parse_method(args.method)
    if args.quick:
        configs = [
            {"label": "1D", "sides": [96], "max_level": 6},
            {"label": "2D", "sides": [18, 18], "max_level": 6},
            {"label": "3D", "sides": [8, 8, 8], "max_level": 4},
            {"label": "4D", "sides": [6, 6, 6, 6], "max_level": 3},
        ]
    elif args.large:
        configs = [
            {"label": "1D", "sides": [32768], "max_level": 8, "sample_nodes": 64},
            {"label": "2D", "sides": [224, 224], "max_level": 8, "sample_nodes": 64},
            {"label": "3D", "sides": [32, 32, 32], "max_level": 6, "sample_nodes": 48},
            {"label": "4D", "sides": [14, 14, 14, 14], "max_level": 4, "sample_nodes": 32},
        ]
    else:
        configs = [
            {"label": "1D", "sides": [256], "max_level": 8},
            {"label": "2D", "sides": [32, 32], "max_level": 8},
            {"label": "3D", "sides": [12, 12, 12], "max_level": 6},
            {"label": "4D", "sides": [8, 8, 8, 8], "max_level": 4},
        ]

    rows = measure_configs(configs, sample_nodes=max(1, args.sample_nodes), method=method, order=max(1, args.order))

    print("\nMaximum dimension summary")
    print(format_table(rows))
    for row in rows:
        curve = ", ".join(f"{v:.3f}" for v in row["local_curve"])
        print(f"\n{row['label']} local d_i(r): [{curve}]")

    if not args.skip_plot:
        plot_curves(rows, args.output, args.show)


if __name__ == "__main__":
    main()
