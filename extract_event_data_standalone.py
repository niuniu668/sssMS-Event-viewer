#!/usr/bin/env python3
"""Standalone tool to recover event data files from mixed folders.

Features:
1) Parse event time from event folder name: yyyy.MM.dd.HH.mm[.ss[.mmm[-x]]]
2) Recursively scan data files from a root folder
3) Filter candidate minute files by file creation/modified time
4) Read waveforms (binary or text)
5) Cut event window (default: pre 1000 + post 3000 @ 4000 Hz)
6) Group files by folder prior and waveform similarity
7) Export cut windows and summary report
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import sys
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

try:
    import numpy as np
except Exception as exc:  # pragma: no cover
    print(f"[ERROR] numpy is required: {exc}", file=sys.stderr)
    sys.exit(2)

try:
    import matplotlib
    matplotlib.use('Agg')  # non-interactive backend
    import matplotlib.pyplot as plt
except Exception as exc:  # pragma: no cover
    plt = None
    print(f"[WARN] matplotlib not available, skipping visualization: {exc}", file=sys.stderr)

EVENT_DIR_RE = re.compile(
    r"^(\d{4})\.(\d{2})\.(\d{2})\.(\d{2})\.(\d{2})(?:\.(\d{2})(?:\.(\d{3})(?:-\d+)?)?)?$"
)
TEXT_SPLIT_RE = re.compile(r"[,\s]+")


@dataclass
class CandidateFile:
    path: str
    rel_parent: str
    ctime: float
    mtime: float
    time_score_sec: float


@dataclass
class ExtractionResult:
    path: str
    rel_parent: str
    ctime_iso: str
    mtime_iso: str
    time_score_sec: float
    channels: int
    sample_count: int
    cut_file: str
    cluster_id: int
    corr_to_cluster_center: float
    folder_group_rank: int


@dataclass
class ScanSummary:
    event_folder: str
    search_root: str
    output_dir: str
    event_time: str
    total_files_scanned: int
    time_candidates: int
    extracted_files: int


@dataclass
class BestPick:
    cluster_id: int
    source_path: str
    rel_parent: str
    time_score_sec: float
    corr_score: float
    channels: int
    sample_count: int
    rank_in_cluster: int
    total_in_cluster: int
    cut_file: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Find and extract event-related data files from mixed folders."
    )
    parser.add_argument("event_folder", help="Event folder path (its name should contain event time).")
    parser.add_argument(
        "--search-root",
        default=None,
        help="Root folder to recursively scan data files. Default: parent of event folder.",
    )
    parser.add_argument(
        "--output-dir",
        default=None,
        help="Output directory. Default: <event_folder>/extracted_by_event_<timestamp>",
    )
    parser.add_argument("--sample-rate", type=int, default=4000, help="Sample rate (Hz).")
    parser.add_argument("--pre", type=int, default=1000, help="Samples before event.")
    parser.add_argument("--post", type=int, default=3000, help="Samples after event.")
    parser.add_argument(
        "--time-window-sec",
        type=float,
        default=90.0,
        help="Max |event_time - file_time| in seconds for candidate files.",
    )
    parser.add_argument(
        "--exts",
        default=".data,.dat,.bin,.txt,.csv,",
        help="Comma-separated allowed suffixes. Include empty token to allow extensionless files.",
    )
    parser.add_argument(
        "--corr-threshold",
        type=float,
        default=0.82,
        help="Correlation threshold for waveform clustering.",
    )
    parser.add_argument(
        "--max-candidates",
        type=int,
        default=5000,
        help="Safety limit for time-filtered candidates.",
    )
    return parser.parse_args()


def parse_event_time(event_folder: Path) -> datetime:
    m = EVENT_DIR_RE.match(event_folder.name)
    if not m:
        raise ValueError(
            "Cannot parse event time from folder name. Expected format: "
            "yyyy.MM.dd.HH.mm[.ss[.mmm[-x]]]"
        )

    y = int(m.group(1))
    mon = int(m.group(2))
    d = int(m.group(3))
    hh = int(m.group(4))
    mm = int(m.group(5))
    ss = int(m.group(6) or "0")
    msec = int(m.group(7) or "0")
    return datetime(y, mon, d, hh, mm, ss, msec * 1000)


def normalize_exts(raw: str) -> Tuple[set[str], bool]:
    allow_no_suffix = False
    ext_set: set[str] = set()
    for token in raw.split(","):
        t = token.strip().lower()
        if t == "":
            allow_no_suffix = True
            continue
        if not t.startswith("."):
            t = "." + t
        ext_set.add(t)
    return ext_set, allow_no_suffix


def iter_files(root: Path) -> Iterable[Path]:
    for p in root.rglob("*"):
        if p.is_file():
            yield p


def to_iso(ts: float) -> str:
    return datetime.fromtimestamp(ts).isoformat(timespec="milliseconds")


def collect_candidates(
    root: Path,
    event_time: datetime,
    allowed_exts: set[str],
    allow_no_suffix: bool,
    time_window_sec: float,
    max_candidates: int,
) -> Tuple[List[CandidateFile], int]:
    total_scanned = 0
    cands: List[CandidateFile] = []
    event_ts = event_time.timestamp()

    for fp in iter_files(root):
        total_scanned += 1
        suffix = fp.suffix.lower()
        if suffix:
            if suffix not in allowed_exts:
                continue
        elif not allow_no_suffix:
            continue

        try:
            st = fp.stat()
        except OSError:
            continue

        ctime = st.st_ctime
        mtime = st.st_mtime
        score = min(abs(ctime - event_ts), abs(mtime - event_ts))
        if score > time_window_sec:
            continue

        rel_parent = str(fp.parent.relative_to(root)) if fp.parent != root else "."
        cands.append(
            CandidateFile(
                path=str(fp),
                rel_parent=rel_parent,
                ctime=ctime,
                mtime=mtime,
                time_score_sec=score,
            )
        )

    cands.sort(key=lambda x: (x.time_score_sec, x.rel_parent.lower(), x.path.lower()))
    if len(cands) > max_candidates:
        cands = cands[:max_candidates]
    return cands, total_scanned


def read_text_wave(path: Path) -> Optional[np.ndarray]:
    rows: List[List[float]] = []
    is_three: Optional[bool] = None

    try:
        with path.open("r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                parts = [x for x in TEXT_SPLIT_RE.split(line) if x]
                vals: List[float] = []
                for p in parts:
                    try:
                        vals.append(float(p))
                    except ValueError:
                        pass
                if not vals:
                    continue

                if is_three is None:
                    is_three = len(vals) >= 3

                if is_three:
                    row = [0.0, 0.0, 0.0]
                    for i in range(min(3, len(vals))):
                        row[i] = vals[i]
                    rows.append(row)
                else:
                    rows.append([vals[0]])
    except OSError:
        return None

    if not rows:
        return None
    return np.asarray(rows, dtype=np.float64)


def read_binary_wave(path: Path) -> Optional[np.ndarray]:
    try:
        raw = path.read_bytes()
    except OSError:
        return None

    double_count = len(raw) // 8
    if double_count <= 6:
        return None

    arr = np.frombuffer(raw[: double_count * 8], dtype="<f8")
    arr = arr[6:]  # skip header doubles to match existing C++ logic

    is_three = len(raw) > 43 and raw[43] == 3
    if is_three:
        usable = (arr.size // 3) * 3
        if usable <= 0:
            return None
        return arr[:usable].reshape(-1, 3)
    return arr.reshape(-1, 1)


def read_wave(path: Path) -> Optional[np.ndarray]:
    txt = read_text_wave(path)
    if txt is not None:
        return txt
    return read_binary_wave(path)


def cut_event_window(
    data: np.ndarray,
    event_time: datetime,
    sample_rate: int,
    pre: int,
    post: int,
) -> np.ndarray:
    channels = data.shape[1]
    total = pre + post

    sample_index = event_time.second * sample_rate + (event_time.microsecond * sample_rate) // 1_000_000
    start = sample_index - pre
    end = sample_index + post

    out = np.zeros((total, channels), dtype=np.float64)
    src_start = max(0, start)
    src_end = min(data.shape[0], end)
    if src_end > src_start:
        dst_start = src_start - start
        dst_end = dst_start + (src_end - src_start)
        out[dst_start:dst_end] = data[src_start:src_end]
    return out


def zscore_1d(x: np.ndarray) -> np.ndarray:
    x = np.asarray(x, dtype=np.float64)
    mu = float(np.mean(x))
    sd = float(np.std(x))
    if sd < 1e-12:
        return np.zeros_like(x)
    return (x - mu) / sd


def corr(a: np.ndarray, b: np.ndarray) -> float:
    az = zscore_1d(a)
    bz = zscore_1d(b)
    den = float(np.linalg.norm(az) * np.linalg.norm(bz))
    if den < 1e-12:
        return 0.0
    return float(np.dot(az, bz) / den)


def cluster_by_similarity(
    series_list: Sequence[np.ndarray],
    rel_parents: Sequence[str],
    corr_threshold: float,
) -> Tuple[List[int], List[float]]:
    centers: List[np.ndarray] = []
    cluster_ids: List[int] = []
    corr_scores: List[float] = []

    # Folder prior: process files in folder-major order so same-level files naturally cluster first.
    order = sorted(range(len(series_list)), key=lambda i: (rel_parents[i].lower(), i))
    inv = [0] * len(order)
    for ranked_pos, original_idx in enumerate(order):
        inv[ranked_pos] = original_idx

    temp_ids = [0] * len(series_list)
    temp_scores = [0.0] * len(series_list)

    for idx in order:
        s = series_list[idx]
        best_id = -1
        best_c = -1.0
        for cid, center in enumerate(centers):
            c = corr(s, center)
            if c > best_c:
                best_c = c
                best_id = cid

        if best_id >= 0 and best_c >= corr_threshold:
            temp_ids[idx] = best_id
            temp_scores[idx] = best_c
            centers[best_id] = 0.7 * centers[best_id] + 0.3 * s
        else:
            centers.append(s.copy())
            temp_ids[idx] = len(centers) - 1
            temp_scores[idx] = 1.0

    for i in range(len(series_list)):
        cluster_ids.append(temp_ids[i])
        corr_scores.append(temp_scores[i])

    return cluster_ids, corr_scores


def write_cut_csv(path: Path, arr: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        cols = [f"ch{i+1}" for i in range(arr.shape[1])]
        writer.writerow(cols)
        writer.writerows(arr.tolist())


def select_best_picks(
    rows: List[ExtractionResult],
    extracted_arrays: List[np.ndarray],
) -> List[BestPick]:
    """Select the most trustworthy file for each cluster.
    
    Score = 0.6 * (1 - time_score/120) + 0.4 * corr_score
    """
    cluster_groups: Dict[int, List[Tuple[ExtractionResult, int, float]]] = {}
    for idx, row in enumerate(rows):
        cid = row.cluster_id
        if cid not in cluster_groups:
            cluster_groups[cid] = []
        
        # Normalize: lower time_score is better, corr_score is already [0,1]
        time_norm = min(1.0, row.time_score_sec / 120.0)
        score = 0.6 * (1.0 - time_norm) + 0.4 * row.corr_to_cluster_center
        cluster_groups[cid].append((row, idx, score))
    
    best_picks: List[BestPick] = []
    for cid in sorted(cluster_groups.keys()):
        group = cluster_groups[cid]
        group.sort(key=lambda x: x[2], reverse=True)  # higher score first
        
        for rank, (row, _, score) in enumerate(group, start=1):
            if rank == 1:  # only the best one
                best_picks.append(
                    BestPick(
                        cluster_id=cid,
                        source_path=row.path,
                        rel_parent=row.rel_parent,
                        time_score_sec=row.time_score_sec,
                        corr_score=row.corr_to_cluster_center,
                        channels=row.channels,
                        sample_count=row.sample_count,
                        rank_in_cluster=rank,
                        total_in_cluster=len(group),
                        cut_file=row.cut_file,
                    )
                )
    return best_picks


def generate_cluster_visualizations(
    rows: List[ExtractionResult],
    extracted_arrays: List[np.ndarray],
    out_dir: Path,
    event_time: datetime,
) -> int:
    """Generate stacked waveform plots for each cluster."""
    if plt is None:
        print("[WARN] matplotlib not available, skipping visualizations")
        return 0
    
    cluster_groups: Dict[int, List[Tuple[int, np.ndarray, str]]] = {}
    for idx, (row, arr) in enumerate(zip(rows, extracted_arrays)):
        cid = row.cluster_id
        if cid not in cluster_groups:
            cluster_groups[cid] = []
        
        label = f"{Path(row.path).name} ({row.time_score_sec:.1f}s, r={row.corr_to_cluster_center:.3f})"
        cluster_groups[cid].append((idx, arr, label))
    
    viz_dir = out_dir / "cluster_visualizations"
    viz_dir.mkdir(parents=True, exist_ok=True)
    
    for cid in sorted(cluster_groups.keys()):
        group = cluster_groups[cid]
        fig, axes = plt.subplots(len(group), 1, figsize=(14, 3 * len(group)))
        if len(group) == 1:
            axes = [axes]
        
        fig.suptitle(f"Cluster {cid} ({len(group)} candidates)", fontsize=14, fontweight="bold")
        
        for ax_idx, (orig_idx, arr, label) in enumerate(group):
            ax = axes[ax_idx]
            
            for ch in range(min(3, arr.shape[1])):
                color = ["red", "green", "blue"][ch]
                ch_label = f"ch{ch+1}" if arr.shape[1] == 1 else ["X", "Y", "Z"][ch]
                ax.plot(arr[:, ch], label=ch_label, color=color, alpha=0.7, linewidth=0.8)
            
            # Mark event time
            event_sample = 1000  # default pre=1000
            ax.axvline(x=event_sample, color="black", linestyle="--", linewidth=1, alpha=0.5, label="event")
            
            ax.set_title(label, fontsize=10)
            ax.set_xlabel("Sample")
            ax.set_ylabel("Amplitude")
            ax.legend(loc="best", fontsize=8)
            ax.grid(True, alpha=0.3)
        
        plt.tight_layout()
        out_path = viz_dir / f"cluster_{cid:03d}.png"
        plt.savefig(out_path, dpi=100, bbox_inches="tight")
        plt.close(fig)
    
    return len(cluster_groups)


def write_best_picks_report(best_picks: List[BestPick], out_dir: Path) -> None:
    """Write best picks summary and recommendation list."""
    best_csv = out_dir / "best_picks_recommendation.csv"
    with best_csv.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(asdict(best_picks[0]).keys()))
        writer.writeheader()
        for bp in sorted(best_picks, key=lambda x: x.cluster_id):
            writer.writerow(asdict(bp))
    
    best_txt = out_dir / "best_picks_summary.txt"
    with best_txt.open("w", encoding="utf-8") as f:
        f.write("=" * 80 + "\n")
        f.write("BEST PICKS RECOMMENDATION (Most Trustworthy File per Cluster)\n")
        f.write("=" * 80 + "\n\n")
        
        for bp in sorted(best_picks, key=lambda x: x.cluster_id):
            f.write(f"[Cluster {bp.cluster_id}] Rank {bp.rank_in_cluster}/{bp.total_in_cluster}\n")
            f.write(f"  Source:     {bp.source_path}\n")
            f.write(f"  Folder:     {bp.rel_parent}\n")
            f.write(f"  Time diff:  {bp.time_score_sec:.3f} seconds\n")
            f.write(f"  Corr score: {bp.corr_score:.4f}\n")
            f.write(f"  Channels:   {bp.channels}, Samples: {bp.sample_count}\n")
            f.write(f"  Cut file:   {bp.cut_file}\n")
            f.write("\n")


def run() -> int:
    args = parse_args()

    event_folder = Path(args.event_folder).expanduser().resolve()
    if not event_folder.exists() or not event_folder.is_dir():
        print(f"[ERROR] Invalid event folder: {event_folder}", file=sys.stderr)
        return 2

    try:
        event_time = parse_event_time(event_folder)
    except ValueError as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 2

    search_root = Path(args.search_root).expanduser().resolve() if args.search_root else event_folder.parent
    if not search_root.exists() or not search_root.is_dir():
        print(f"[ERROR] Invalid search root: {search_root}", file=sys.stderr)
        return 2

    out_dir = (
        Path(args.output_dir).expanduser().resolve()
        if args.output_dir
        else event_folder / f"extracted_by_event_{event_time.strftime('%Y%m%d_%H%M%S_%f')[:-3]}"
    )
    out_dir.mkdir(parents=True, exist_ok=True)

    exts, allow_no_suffix = normalize_exts(args.exts)

    print("[1/4] Scanning files...")
    candidates, total_scanned = collect_candidates(
        root=search_root,
        event_time=event_time,
        allowed_exts=exts,
        allow_no_suffix=allow_no_suffix,
        time_window_sec=args.time_window_sec,
        max_candidates=args.max_candidates,
    )
    print(f"      scanned={total_scanned}, time_candidates={len(candidates)}")

    print("[2/4] Reading and cutting waveforms...")
    extracted_arrays: List[np.ndarray] = []
    meta: List[CandidateFile] = []

    for cand in candidates:
        p = Path(cand.path)
        data = read_wave(p)
        if data is None or data.size == 0:
            continue

        cut = cut_event_window(
            data=data,
            event_time=event_time,
            sample_rate=args.sample_rate,
            pre=args.pre,
            post=args.post,
        )
        extracted_arrays.append(cut)
        meta.append(cand)

    if not extracted_arrays:
        print("[WARN] No readable data files after filtering.")
        summary = ScanSummary(
            event_folder=str(event_folder),
            search_root=str(search_root),
            output_dir=str(out_dir),
            event_time=event_time.isoformat(timespec="milliseconds"),
            total_files_scanned=total_scanned,
            time_candidates=len(candidates),
            extracted_files=0,
        )
        (out_dir / "summary.json").write_text(
            json.dumps(asdict(summary), indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
        return 0

    print("[3/4] Clustering by waveform similarity...")
    first_channels = [arr[:, 0] for arr in extracted_arrays]
    rel_parents = [m.rel_parent for m in meta]
    cluster_ids, corr_scores = cluster_by_similarity(
        first_channels,
        rel_parents,
        corr_threshold=args.corr_threshold,
    )

    folder_rank: Dict[str, int] = {}
    for parent in rel_parents:
        folder_rank[parent] = folder_rank.get(parent, 0) + 1

    print("[4/4] Writing outputs...")
    rows: List[ExtractionResult] = []
    cuts_dir = out_dir / "cuts"
    for idx, (cand, cut, cid, cscore) in enumerate(zip(meta, extracted_arrays, cluster_ids, corr_scores), start=1):
        src_name = Path(cand.path).name
        safe_parent = cand.rel_parent.replace("\\", "_").replace("/", "_").replace(":", "_")
        cut_name = f"{idx:04d}_cluster{cid}_{safe_parent}_{src_name}.csv"
        cut_path = cuts_dir / cut_name
        write_cut_csv(cut_path, cut)

        rows.append(
            ExtractionResult(
                path=cand.path,
                rel_parent=cand.rel_parent,
                ctime_iso=to_iso(cand.ctime),
                mtime_iso=to_iso(cand.mtime),
                time_score_sec=round(cand.time_score_sec, 3),
                channels=int(cut.shape[1]),
                sample_count=int(cut.shape[0]),
                cut_file=str(cut_path),
                cluster_id=int(cid),
                corr_to_cluster_center=round(float(cscore), 4),
                folder_group_rank=int(folder_rank.get(cand.rel_parent, 0)),
            )
        )

    results_csv = out_dir / "extracted_index.csv"
    with results_csv.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(asdict(rows[0]).keys()))
        writer.writeheader()
        for r in rows:
            writer.writerow(asdict(r))

    summary = ScanSummary(
        event_folder=str(event_folder),
        search_root=str(search_root),
        output_dir=str(out_dir),
        event_time=event_time.isoformat(timespec="milliseconds"),
        total_files_scanned=total_scanned,
        time_candidates=len(candidates),
        extracted_files=len(rows),
    )

    (out_dir / "summary.json").write_text(
        json.dumps(
            {
                "summary": asdict(summary),
                "clusters": {
                    "count": int(max(cluster_ids) + 1 if cluster_ids else 0),
                    "corr_threshold": args.corr_threshold,
                },
            },
            indent=2,
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )

    print("[5/6] Generating best picks recommendation...")
    best_picks = select_best_picks(rows, extracted_arrays)
    write_best_picks_report(best_picks, out_dir)
    print(f"      best_picks: {len(best_picks)}")

    print("[6/6] Generating cluster visualizations...")
    cluster_count = generate_cluster_visualizations(rows, extracted_arrays, out_dir, event_time)
    print(f"      clusters visualized: {cluster_count}")

    print("[DONE]")
    print(f"  output_dir: {out_dir}")
    print(f"  extracted_index: {results_csv}")
    print(f"  best_picks: {out_dir / 'best_picks_recommendation.csv'}")
    print(f"  best_picks_summary: {out_dir / 'best_picks_summary.txt'}")
    print(f"  visualizations: {out_dir / 'cluster_visualizations'}")
    print(f"  cuts_dir: {cuts_dir}")
    print(f"  extracted_files: {len(rows)}")
    print(f"  best_picks: {len(best_picks)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
