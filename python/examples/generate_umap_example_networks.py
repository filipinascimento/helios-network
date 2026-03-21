from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np

from helios_network import AttributeScope, AttributeType, HeliosUMAP


DEFAULT_SIZES = (200, 2000, 20000)
DEFAULT_CLUSTER_COUNT = 8
DEFAULT_FEATURE_DIMS = 16
DEFAULT_SEED = 13
DEFAULT_OUTPUT_DIR = (
    Path(__file__).resolve().parents[3]
    / "helios-web-next"
    / "public"
    / "assets"
    / "umap"
)


@dataclass(slots=True)
class ExportRecord:
    name: str
    node_count: int
    cluster_count: int
    feature_dims: int
    seed: int
    format: str
    path: str
    n_neighbors: int
    min_dist: float
    negative_sample_rate: float


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate real HeliosUMAP-exported example networks for helios-web-next.",
    )
    parser.add_argument(
        "--sizes",
        type=int,
        nargs="+",
        default=list(DEFAULT_SIZES),
        help="Node counts to export.",
    )
    parser.add_argument(
        "--cluster-count",
        type=int,
        default=DEFAULT_CLUSTER_COUNT,
        help="Number of Gaussian clusters.",
    )
    parser.add_argument(
        "--feature-dims",
        type=int,
        default=DEFAULT_FEATURE_DIMS,
        help="Feature dimensions for the synthetic high-dimensional data.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=DEFAULT_SEED,
        help="Base random seed.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help="Directory where .zxnet exports will be written.",
    )
    parser.add_argument(
        "--compression",
        type=int,
        default=6,
        help="Compression level for .zxnet exports.",
    )
    return parser.parse_args()


def _cluster_centers(cluster_count: int, feature_dims: int) -> np.ndarray:
    centers = np.zeros((cluster_count, feature_dims), dtype=np.float32)
    for cluster_id in range(cluster_count):
        angle = (2.0 * np.pi * cluster_id) / max(1, cluster_count)
        centers[cluster_id, 0] = np.cos(angle) * 9.0
        centers[cluster_id, 1] = np.sin(angle) * 9.0
        centers[cluster_id, 2] = np.cos(angle * 2.0) * 4.5
        centers[cluster_id, 3] = np.sin(angle * 2.0) * 4.5
        for dim in range(4, feature_dims):
            centers[cluster_id, dim] = ((cluster_id + 1) * (dim + 3) % 11) * 0.35
    return centers


def _cluster_sizes(node_count: int, cluster_count: int) -> list[int]:
    base = node_count // cluster_count
    remainder = node_count % cluster_count
    return [base + (1 if index < remainder else 0) for index in range(cluster_count)]


def build_clustered_dataset(
    node_count: int,
    *,
    cluster_count: int,
    feature_dims: int,
    seed: int,
) -> tuple[np.ndarray, np.ndarray]:
    rng = np.random.default_rng(seed)
    centers = _cluster_centers(cluster_count, feature_dims)
    sizes = _cluster_sizes(node_count, cluster_count)

    parts: list[np.ndarray] = []
    labels: list[np.ndarray] = []
    for cluster_id, size in enumerate(sizes):
        if size <= 0:
            continue
        scale = 0.45 + (cluster_id % 3) * 0.05
        samples = rng.normal(
            loc=centers[cluster_id],
            scale=scale,
            size=(size, feature_dims),
        ).astype(np.float32, copy=False)
        samples[:, 0] += rng.normal(0.0, 0.2, size=size).astype(np.float32, copy=False)
        samples[:, 1] += rng.normal(0.0, 0.2, size=size).astype(np.float32, copy=False)
        parts.append(samples)
        labels.append(np.full(size, cluster_id, dtype=np.uint32))

    features = np.concatenate(parts, axis=0)
    cluster_ids = np.concatenate(labels, axis=0)

    order = rng.permutation(node_count)
    return (
        np.ascontiguousarray(features[order], dtype=np.float32),
        np.ascontiguousarray(cluster_ids[order], dtype=np.uint32),
    )


def annotate_exported_network(network, cluster_ids: np.ndarray, *, dataset_name: str) -> None:
    cluster_labels = [f"cluster-{int(cluster_id)}" for cluster_id in cluster_ids.tolist()]
    node_labels = [f"{dataset_name}-node-{index}" for index in range(int(cluster_ids.shape[0]))]
    mass = np.asarray(network.nodes["umap_mass"], dtype=np.float32)
    max_mass = float(np.max(mass)) if mass.size else 1.0
    if not np.isfinite(max_mass) or max_mass <= 0:
        max_mass = 1.0
    weight = np.ascontiguousarray((mass / max_mass).astype(np.float32, copy=False))

    network.define_attribute(AttributeScope.Node, "test_cluster", AttributeType.UnsignedInteger, 1)
    network.define_attribute(AttributeScope.Node, "category", AttributeType.String, 1)
    network.define_attribute(AttributeScope.Node, "label", AttributeType.String, 1)
    network.define_attribute(AttributeScope.Node, "weight", AttributeType.Float, 1)
    network.nodes["test_cluster"] = cluster_ids.tolist()
    network.nodes["category"] = cluster_labels
    network.nodes["label"] = node_labels
    network.nodes["weight"] = weight.tolist()
    network["umap_example_name"] = dataset_name
    network["umap_example_cluster_count"] = int(np.max(cluster_ids, initial=0) + 1)
    network["umap_example_node_count"] = int(cluster_ids.shape[0])


def export_case(
    node_count: int,
    *,
    cluster_count: int,
    feature_dims: int,
    seed: int,
    output_dir: Path,
    compression: int,
) -> ExportRecord:
    case_seed = seed + node_count
    dataset_name = f"gaussian-{node_count}"
    features, cluster_ids = build_clustered_dataset(
        node_count,
        cluster_count=cluster_count,
        feature_dims=feature_dims,
        seed=case_seed,
    )

    exporter = HeliosUMAP(
        n_neighbors=15,
        min_dist=0.08,
        spread=1.0,
        n_components=2,
        metric="euclidean",
        negative_sample_rate=5.0,
        repulsion_strength=1.0,
        init="spectral",
        random_state=case_seed,
        transform_seed=case_seed,
        low_memory=True,
        build_knn_network=False,
        prefer_pynndescent=True,
    )
    network = exporter.fit_graph_network(features)
    annotate_exported_network(network, cluster_ids, dataset_name=dataset_name)

    target = output_dir / f"{dataset_name}.zxnet"
    network.save_zxnet(str(target), compression)

    return ExportRecord(
        name=dataset_name,
        node_count=node_count,
        cluster_count=cluster_count,
        feature_dims=feature_dims,
        seed=case_seed,
        format="zxnet",
        path=target.name,
        n_neighbors=15,
        min_dist=0.08,
        negative_sample_rate=5.0,
    )


def main() -> int:
    args = _parse_args()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    records: list[ExportRecord] = []
    for node_count in args.sizes:
        print(f"[helios-umap-export] generating {node_count} nodes...")
        record = export_case(
            int(node_count),
            cluster_count=max(2, int(args.cluster_count)),
            feature_dims=max(4, int(args.feature_dims)),
            seed=int(args.seed),
            output_dir=output_dir,
            compression=max(0, min(9, int(args.compression))),
        )
        records.append(record)
        print(f"[helios-umap-export] wrote {record.path}")

    manifest = {
        "datasets": [asdict(record) for record in records],
    }
    manifest_path = output_dir / "index.json"
    manifest_path.write_text(f"{json.dumps(manifest, indent=2)}\n", encoding="utf-8")
    print(f"[helios-umap-export] wrote {manifest_path.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
