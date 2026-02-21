from __future__ import annotations

import argparse
import math
from pathlib import Path

from helios_network import DimensionMethod, Network


METHODS = [
    ("2-dCE", DimensionMethod.Central, "#1f77b4"),
    ("2-dBK", DimensionMethod.Backward, "#ff7f0e"),
    ("2-dFW", DimensionMethod.Forward, "#2ca02c"),
    ("2-LS", DimensionMethod.LeastSquares, "#d62728"),
]

DIMENSION_MARKERS = {
    1: "s",
    2: "o",
    3: "^",
    4: "D",
}

SVG_MARKERS = {
    "s": "square",
    "o": "circle",
    "^": "triangle",
    "D": "diamond",
}


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


def create_toroidal_network(
    sides: list[int],
    node_batch_size: int = 250_000,
    edge_batch_size: int = 400_000,
) -> Network:
    total_nodes = 1
    for side in sides:
        total_nodes *= side

    network = Network(directed=False)
    added = 0
    while added < total_nodes:
        count = min(node_batch_size, total_nodes - added)
        network.add_nodes(count)
        added += count
    strides = strides_from_sides(sides)

    edges: list[tuple[int, int]] = []
    for idx in range(total_nodes):
        coords = coordinates_from_linear(idx, sides)
        for d in range(len(sides)):
            neighbor = coords.copy()
            neighbor[d] = (neighbor[d] + 1) % sides[d]
            n_idx = linear_from_coordinates(neighbor, strides)
            edges.append((idx, n_idx))
            if len(edges) >= edge_batch_size:
                network.add_edges(edges)
                edges.clear()
    if edges:
        network.add_edges(edges)
    return network


def network_configs(mode: str) -> list[dict]:
    if mode == "quick":
        return [
            {"d": 1, "sides": [512]},
            {"d": 2, "sides": [40, 40]},
            {"d": 3, "sides": [12, 12, 12]},
            {"d": 4, "sides": [8, 8, 8, 8]},
        ]
    if mode == "large":
        return [
            {"d": 1, "sides": [32768]},
            {"d": 2, "sides": [224, 224]},
            {"d": 3, "sides": [32, 32, 32]},
            {"d": 4, "sides": [14, 14, 14, 14]},
        ]
    if mode == "xlarge":
        return [
            {"d": 1, "sides": [65536]},
            {"d": 2, "sides": [320, 320]},
            {"d": 3, "sides": [40, 40, 40]},
            {"d": 4, "sides": [24, 24, 24, 24]},
        ]
    if mode == "xxlarge":
        return [
            {"d": 1, "sides": [131072]},
            {"d": 2, "sides": [448, 448]},
            {"d": 3, "sides": [48, 48, 48]},
            {"d": 4, "sides": [28, 28, 28, 28]},
        ]
    if mode == "five_x":
        return [
            {"d": 1, "sides": [655360]},
            {"d": 2, "sides": [1000, 1000]},
            {"d": 3, "sides": [82, 82, 82]},
            {"d": 4, "sides": [42, 42, 42, 42]},
        ]
    if mode == "ten_x":
        return [
            {"d": 1, "sides": [1310720]},
            {"d": 2, "sides": [1414, 1414]},
            {"d": 3, "sides": [104, 104, 104]},
            {"d": 4, "sides": [50, 50, 50, 50]},
        ]
    return [
        {"d": 1, "sides": [4096]},
        {"d": 2, "sides": [128, 128]},
        {"d": 3, "sides": [24, 24, 24]},
        {"d": 4, "sides": [12, 12, 12, 12]},
    ]


def torus_diameter(sides: list[int]) -> int:
    return sum(side // 2 for side in sides)


def measure_average_curves(configs: list[dict], sample_nodes: int, requested_max_level: int) -> list[dict]:
    rows: list[dict] = []
    for config in configs:
        sides = config["sides"]
        d = config["d"]
        network = create_toroidal_network(sides)

        node_count = network.node_count()
        edge_count = network.edge_count()
        diameter = torus_diameter(sides)
        max_level = min(requested_max_level, diameter - 1 if diameter > 1 else 1)
        sampled_count = min(node_count, max(1, sample_nodes))
        sampled_nodes = list(range(sampled_count))

        curves: dict[str, list[float]] = {}
        maxima: dict[str, float] = {}
        for label, method, _color in METHODS:
            stats = network.measure_dimension(
                max_level=max_level,
                method=method,
                order=2,
                nodes=sampled_nodes,
            )
            curve = [float(v) for v in stats["average_node_dimension"]]
            curves[label] = curve
            maxima[label] = max(curve)

        rows.append(
            {
                "d": d,
                "sides": sides,
                "node_count": node_count,
                "edge_count": edge_count,
                "diameter": diameter,
                "max_level": max_level,
                "curves": curves,
                "maxima": maxima,
            }
        )
    return rows


def _svg_marker(x: float, y: float, marker: str, color: str, size: float = 4.0) -> str:
    if marker == "circle":
        return f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{size:.2f}" fill="{color}" />'
    if marker == "square":
        s = size * 1.8
        return f'<rect x="{x - s / 2:.2f}" y="{y - s / 2:.2f}" width="{s:.2f}" height="{s:.2f}" fill="{color}" />'
    if marker == "triangle":
        p1 = (x, y - size * 1.6)
        p2 = (x - size * 1.5, y + size * 1.2)
        p3 = (x + size * 1.5, y + size * 1.2)
        return f'<polygon points="{p1[0]:.2f},{p1[1]:.2f} {p2[0]:.2f},{p2[1]:.2f} {p3[0]:.2f},{p3[1]:.2f}" fill="{color}" />'
    if marker == "diamond":
        p1 = (x, y - size * 1.8)
        p2 = (x - size * 1.5, y)
        p3 = (x, y + size * 1.8)
        p4 = (x + size * 1.5, y)
        return f'<polygon points="{p1[0]:.2f},{p1[1]:.2f} {p2[0]:.2f},{p2[1]:.2f} {p3[0]:.2f},{p3[1]:.2f} {p4[0]:.2f},{p4[1]:.2f}" fill="{color}" />'
    return ""


def save_svg_plot(rows: list[dict], output: Path, title: str) -> None:
    width = 1400
    height = 780
    left = 120
    right = 230
    top = 50
    bottom = 110
    plot_w = width - left - right
    plot_h = height - top - bottom

    max_level = max(row["max_level"] for row in rows)
    y_max_raw = max(max(row["maxima"].values()) for row in rows)
    y_max = max(1.0, math.ceil((y_max_raw + 0.4) * 2.0) / 2.0)

    def x_to_px(x: float) -> float:
        return left + (x / max_level) * plot_w if max_level > 0 else left

    def y_to_px(y: float) -> float:
        return top + plot_h - ((y / y_max) * plot_h if y_max > 0 else 0)

    svg: list[str] = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">')
    svg.append('<rect x="0" y="0" width="100%" height="100%" fill="#ffffff" />')

    for gy in range(0, int(y_max) + 1):
        y = y_to_px(float(gy))
        svg.append(f'<line x1="{left}" y1="{y:.2f}" x2="{left + plot_w}" y2="{y:.2f}" stroke="#e5e7eb" stroke-width="1" />')
        svg.append(f'<text x="{left - 16}" y="{y + 6:.2f}" text-anchor="end" font-size="16" fill="#222">{gy}</text>')

    xtick = 2 if max_level >= 20 else 1
    for gx in range(0, max_level + 1, xtick):
        x = x_to_px(float(gx))
        svg.append(f'<line x1="{x:.2f}" y1="{top + plot_h}" x2="{x:.2f}" y2="{top + plot_h + 8}" stroke="#222" stroke-width="1.5" />')
        svg.append(f'<text x="{x:.2f}" y="{top + plot_h + 32}" text-anchor="middle" font-size="16" fill="#222">{gx}</text>')

    svg.append(f'<line x1="{left}" y1="{top + plot_h}" x2="{left + plot_w}" y2="{top + plot_h}" stroke="#111" stroke-width="2" />')
    svg.append(f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top + plot_h}" stroke="#111" stroke-width="2" />')

    for row in rows:
        d = row["d"]
        marker_shape = SVG_MARKERS[DIMENSION_MARKERS[d]]
        for label, _method, color in METHODS:
            curve = row["curves"][label]
            points = []
            for r in range(1, row["max_level"] + 1):
                yv = curve[r]
                points.append(f"{x_to_px(float(r)):.2f},{y_to_px(yv):.2f}")
            svg.append(f'<polyline points="{" ".join(points)}" fill="none" stroke="{color}" stroke-width="2.2" />')
            for r in range(1, row["max_level"] + 1):
                yv = curve[r]
                svg.append(_svg_marker(x_to_px(float(r)), y_to_px(yv), marker_shape, color, size=3.2))

    legend_x = left + plot_w - 130
    legend_y = top + 25
    for i, (label, _method, color) in enumerate(METHODS):
        y = legend_y + i * 32
        svg.append(f'<line x1="{legend_x}" y1="{y}" x2="{legend_x + 24}" y2="{y}" stroke="{color}" stroke-width="4" stroke-linecap="round" />')
        svg.append(f'<text x="{legend_x + 34}" y="{y + 6}" font-size="20" fill="#222">{label}</text>')

    for row in rows:
        d = row["d"]
        yv = row["curves"]["2-LS"][row["max_level"]]
        y = y_to_px(yv)
        svg.append(f'<text x="{left + plot_w + 28}" y="{y + 6:.2f}" font-size="24" font-style="italic" fill="#222">d={d}</text>')

    svg.append(f'<text x="{left + (plot_w / 2):.2f}" y="{height - 28}" text-anchor="middle" font-size="24" fill="#111">Concentric level r</text>')
    svg.append(
        f'<text x="36" y="{top + (plot_h / 2):.2f}" transform="rotate(-90 36 {top + (plot_h / 2):.2f})" '
        'text-anchor="middle" font-size="40" fill="#111">Average multiscale dimension &lt;d_i(r)&gt;</text>'
    )
    svg.append(f'<text x="{left}" y="30" font-size="22" fill="#111">{title}</text>')
    svg.append("</svg>")

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(svg), encoding="utf-8")


def save_plot(rows: list[dict], output: Path, title: str, show: bool) -> str:
    try:
        import matplotlib.pyplot as plt
        from matplotlib.lines import Line2D
    except Exception:
        if output.suffix.lower() != ".svg":
            output = output.with_suffix(".svg")
        save_svg_plot(rows, output, title)
        return f"svg:{output}"

    plt.figure(figsize=(14, 8))
    for row in rows:
        d = row["d"]
        marker = DIMENSION_MARKERS[d]
        x = list(range(1, row["max_level"] + 1))
        for label, _method, color in METHODS:
            y = [row["curves"][label][r] for r in x]
            plt.plot(x, y, color=color, marker=marker, markersize=5, linewidth=1.8)

    handles = [Line2D([0], [0], color=color, lw=3, label=label) for label, _method, color in METHODS]
    plt.legend(handles=handles, loc="upper right", frameon=False, fontsize=18, handlelength=0.8)
    for row in rows:
        d = row["d"]
        x = row["max_level"]
        y = row["curves"]["2-LS"][x]
        plt.text(x + 0.8, y, f"d={d}", fontsize=24, fontstyle="italic")

    plt.title(title, fontsize=20, loc="left")
    plt.xlabel("Concentric level r", fontsize=28)
    plt.ylabel(r"Average multiscale dimension $\langle d_i(r)\rangle$", fontsize=28)
    plt.xlim(0, max(row["max_level"] for row in rows) + 2)
    plt.ylim(0, max(max(row["maxima"].values()) for row in rows) + 0.5)
    plt.grid(True, alpha=0.25)
    plt.tight_layout()
    output.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(output, dpi=180)
    if show:
        plt.show()
    plt.close()
    return f"matplotlib:{output}"


def print_summary(rows: list[dict]) -> None:
    headers = ["d", "sides", "nodes", "edges", "method", "max <d_i(r)>"]
    body: list[list[str]] = []
    for row in rows:
        sides = "x".join(str(v) for v in row["sides"])
        for label, _method, _color in METHODS:
            body.append(
                [
                    str(row["d"]),
                    sides,
                    str(row["node_count"]),
                    str(row["edge_count"]),
                    label,
                    f"{row['maxima'][label]:.3f}",
                ]
            )

    widths = []
    for col, header in enumerate(headers):
        w = len(header)
        for line in body:
            w = max(w, len(line[col]))
        widths.append(w)

    def pad(values: list[str]) -> str:
        return " | ".join(values[i].ljust(widths[i]) for i in range(len(values)))

    print(pad(headers))
    print("-+-".join("-" * w for w in widths))
    for line in body:
        print(pad(line))


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Plot order-2 CE/BK/FW/LS average multiscale dimension curves on toroidal regular networks."
    )
    parser.add_argument("--quick", action="store_true", help="Use smaller networks for faster runs.")
    parser.add_argument("--large", action="store_true", help="Use larger (20k-50k node) network sizes.")
    parser.add_argument("--xlarge", action="store_true", help="Use extra-large network sizes (up to ~330k nodes).")
    parser.add_argument("--xxlarge", action="store_true", help="Use even larger network sizes (up to ~615k nodes).")
    parser.add_argument("--five-x", action="store_true", help="Use ~5x larger sizes than xxlarge (up to ~3.1M nodes).")
    parser.add_argument("--ten-x", action="store_true", help="Use ~2x larger sizes than five-x (up to ~6.25M nodes).")
    parser.add_argument("--sample-nodes", type=int, default=256, help="Number of sampled nodes used for averaging.")
    parser.add_argument(
        "--max-level",
        type=int,
        default=24,
        help="Requested maximum concentric level r (clipped by each network diameter).",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parent / "toroidal_order2_method_average.svg",
        help="Output image path (.png when matplotlib is available, otherwise .svg fallback).",
    )
    parser.add_argument("--show", action="store_true", help="Display plot window when matplotlib is available.")
    args = parser.parse_args()

    selected = [args.quick, args.large, args.xlarge, args.xxlarge, args.five_x, args.ten_x]
    if sum(1 for flag in selected if flag) > 1:
        raise ValueError("Use only one of --quick, --large, --xlarge, --xxlarge, --five-x, or --ten-x.")

    if args.quick:
        mode = "quick"
    elif args.large:
        mode = "large"
    elif args.xlarge:
        mode = "xlarge"
    elif args.xxlarge:
        mode = "xxlarge"
    elif args.five_x:
        mode = "five_x"
    elif args.ten_x:
        mode = "ten_x"
    else:
        mode = "full"
    configs = network_configs(mode)
    rows = measure_average_curves(
        configs=configs,
        sample_nodes=max(1, args.sample_nodes),
        requested_max_level=max(3, args.max_level),
    )

    print_summary(rows)
    for row in rows:
        print(
            f"d={row['d']} sides={row['sides']} diameter={row['diameter']} "
            f"max_level_used={row['max_level']}"
        )

    title = "Toroidal regular networks: average local dimension, order-2 CE/BK/FW/LS"
    result = save_plot(rows, args.output, title, args.show)
    print(f"\nSaved plot ({result}).")


if __name__ == "__main__":
    main()
