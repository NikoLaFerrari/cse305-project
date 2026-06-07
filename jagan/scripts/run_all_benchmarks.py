#!/usr/bin/env python3

import argparse
import csv
import os
import re
import subprocess
import tempfile
import time
from pathlib import Path


RESULT_FIELDS = [
    "dataset",
    "format",
    "n",
    "dim",
    "target_k",
    "algorithm",
    "mode",
    "threads",
    "status",
    "edge_time",
    "sort_time",
    "mst_time",
    "replay_time",
    "total_time",
    "merges",
    "runtime_wall",
    "error",
]


def is_number(s: str) -> bool:
    try:
        float(s)
        return True
    except Exception:
        return False


def split_line(line: str):
    line = line.strip()
    if "," in line:
        return [x.strip() for x in line.split(",")]
    if ";" in line:
        return [x.strip() for x in line.split(";")]
    if "\t" in line:
        return [x.strip() for x in line.split("\t")]
    return line.split()


def convert_arff_to_csv(arff_path: Path, out_path: Path) -> bool:
    attributes = []
    rows = []
    in_data = False

    try:
        with open(arff_path, "r", encoding="utf-8", errors="ignore") as f:
            for raw in f:
                line = raw.strip()

                if not line or line.startswith("%"):
                    continue

                low = line.lower()

                if low.startswith("@attribute"):
                    parts = line.split()
                    if len(parts) >= 2:
                        attributes.append(parts[1].strip("'\""))
                    continue

                if low.startswith("@data"):
                    in_data = True
                    continue

                if in_data:
                    if "{" in line:
                        return False
                    rows.append([x.strip() for x in line.split(",")])

        if not rows:
            return False

        with open(out_path, "w", newline="") as f:
            writer = csv.writer(f)
            if attributes and len(attributes) == len(rows[0]):
                writer.writerow(attributes)
            writer.writerows(rows)

        return True

    except Exception:
        return False


def inspect_csv_like(path: Path):
    """
    Returns:
        (ok, n, dim, has_header, last_col_label, target_k)
    """
    lines = []

    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            for raw in f:
                line = raw.strip()
                if line and not line.startswith("%") and not line.startswith("#"):
                    lines.append(line)
                if len(lines) >= 2000:
                    break
    except Exception:
        return False, 0, 0, 0, 0, 3

    if not lines:
        return False, 0, 0, 0, 0, 3

    first = split_line(lines[0])
    has_header = not all(is_number(x) for x in first)

    data_lines = lines[1:] if has_header else lines
    if not data_lines:
        return False, 0, 0, int(has_header), 0, 3

    first_data = split_line(data_lines[0])
    if not all(is_number(x) for x in first_data):
        return False, 0, 0, int(has_header), 0, 3

    cols = len(first_data)

    # Count all valid rows, not just sample.
    n = 0
    labels = set()
    valid_rows_checked = 0
    last_col_label = False

    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            for idx, raw in enumerate(f):
                line = raw.strip()
                if not line or line.startswith("%") or line.startswith("#"):
                    continue
                if has_header:
                    has_header = False
                    continue

                cells = split_line(line)
                if len(cells) != cols:
                    continue
                if not all(is_number(x) for x in cells):
                    continue

                n += 1
                valid_rows_checked += 1

                # Heuristic: last column is label if it is integer-ish and low-cardinality.
                last = float(cells[-1])
                if abs(last - round(last)) < 1e-9:
                    labels.add(int(round(last)))

        if n == 0:
            return False, 0, 0, 0, 0, 3

        # Recompute header flag because loop modified it.
        first = split_line(lines[0])
        header_flag = not all(is_number(x) for x in first)

        if 1 < len(labels) <= max(2, min(100, n // 2)):
            last_col_label = True
            target_k = len(labels)
            dim = cols - 1
        else:
            last_col_label = False
            target_k = 3
            dim = cols

        if dim <= 0:
            return False, 0, 0, int(header_flag), int(last_col_label), target_k

        return True, n, dim, int(header_flag), int(last_col_label), target_k

    except Exception:
        return False, 0, 0, 0, 0, 3


def parse_output(text: str):
    def grab(pattern, default=""):
        m = re.search(pattern, text)
        return m.group(1) if m else default

    loaded = re.search(r"Loaded\s+(\d+)\s+points,\s+dimension\s+(\d+)", text)
    n = loaded.group(1) if loaded else ""
    dim = loaded.group(2) if loaded else ""

    return {
        "n": n,
        "dim": dim,
        "edge_time": grab(r"Edge computation:\s+([0-9.eE+-]+)"),
        "sort_time": grab(r"Sorting:\s+([0-9.eE+-]+)"),
        "mst_time": grab(r"MST/Boruvka:\s+([0-9.eE+-]+)"),
        "replay_time": grab(r"Replay:\s+([0-9.eE+-]+)"),
        "total_time": grab(r"Total:\s+([0-9.eE+-]+)"),
        "merges": grab(r"Number of merges:\s+(\d+)"),
    }


def run_one(hac_bin, dataset_path, target_k, algorithm, mode, threads,
            has_header, last_col_label, timeout):
    cmd = [
        hac_bin,
        str(dataset_path),
        str(target_k),
        algorithm,
        mode,
        str(threads),
        str(has_header),
        str(last_col_label),
        "0",
    ]

    start = time.time()

    try:
        proc = subprocess.run(
            cmd,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
        )

        wall = time.time() - start
        text = proc.stdout + "\n" + proc.stderr
        parsed = parse_output(text)

        status = "ok" if proc.returncode == 0 else "error"
        error = "" if proc.returncode == 0 else text.replace("\n", " | ")[:500]

        return status, parsed, wall, error, text

    except subprocess.TimeoutExpired:
        wall = time.time() - start
        return "timeout", {}, wall, f"timeout after {timeout}s", ""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset-root", default="../dataset")
    parser.add_argument("--hac", default="./hac")
    parser.add_argument("--out-csv", default="results/benchmark_results.csv")
    parser.add_argument("--log", default="results/benchmark_log.txt")
    parser.add_argument("--threads", default="1,2,4,8")
    parser.add_argument("--max-n", type=int, default=5000)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--include-arff", action="store_true")
    parser.add_argument("--include-txt", action="store_true")
    args = parser.parse_args()

    root = Path(args.dataset_root)
    threads_list = [int(x) for x in args.threads.split(",") if x.strip()]

    os.makedirs(Path(args.out_csv).parent, exist_ok=True)
    os.makedirs(Path(args.log).parent, exist_ok=True)

    candidates = []
    for p in root.rglob("*"):
        if not p.is_file():
            continue

        suffix = p.suffix.lower()

        if suffix == ".csv":
            candidates.append(p)
        elif suffix == ".arff" and args.include_arff:
            candidates.append(p)
        elif suffix == ".txt" and args.include_txt:
            candidates.append(p)

    candidates = sorted(candidates)

    with open(args.out_csv, "w", newline="") as csvfile, open(args.log, "w") as logfile:
        writer = csv.DictWriter(csvfile, fieldnames=RESULT_FIELDS)
        writer.writeheader()

        logfile.write(f"Dataset root: {root}\n")
        logfile.write(f"Candidates: {len(candidates)}\n")
        logfile.write(f"Threads: {threads_list}\n")
        logfile.write(f"Max n: {args.max_n}\n")
        logfile.write(f"Timeout: {args.timeout}s\n\n")

        with tempfile.TemporaryDirectory() as tmpdir:
            tmpdir = Path(tmpdir)

            for original_path in candidates:
                path = original_path
                fmt = original_path.suffix.lower().lstrip(".")

                if original_path.suffix.lower() == ".arff":
                    converted = tmpdir / (original_path.stem + ".csv")
                    if not convert_arff_to_csv(original_path, converted):
                        logfile.write(f"[SKIP] {original_path}: could not convert ARFF\n")
                        continue
                    path = converted

                ok, n, dim, has_header, last_col_label, target_k = inspect_csv_like(path)

                if not ok:
                    logfile.write(f"[SKIP] {original_path}: not readable numeric CSV/TXT\n")
                    continue

                if n > args.max_n:
                    logfile.write(f"[SKIP] {original_path}: n={n} > max_n={args.max_n}\n")
                    continue

                logfile.write(
                    f"\n[DATASET] {original_path} | n={n}, dim={dim}, "
                    f"k={target_k}, header={has_header}, label={last_col_label}\n"
                )
                logfile.flush()

                jobs = [
                    ("kruskal", "seq", 1),
                    ("boruvka", "seq", 1),
                ]

                for t in threads_list:
                    jobs.append(("kruskal", "par", t))
                    jobs.append(("boruvka", "par", t))

                for algorithm, mode, threads in jobs:
                    logfile.write(f"  Running {algorithm} {mode} threads={threads} ... ")
                    logfile.flush()

                    status, parsed, wall, error, raw_text = run_one(
                        args.hac,
                        path,
                        target_k,
                        algorithm,
                        mode,
                        threads,
                        has_header,
                        last_col_label,
                        args.timeout,
                    )

                    row = {
                        "dataset": str(original_path),
                        "format": fmt,
                        "n": parsed.get("n", n),
                        "dim": parsed.get("dim", dim),
                        "target_k": target_k,
                        "algorithm": algorithm,
                        "mode": mode,
                        "threads": threads,
                        "status": status,
                        "edge_time": parsed.get("edge_time", ""),
                        "sort_time": parsed.get("sort_time", ""),
                        "mst_time": parsed.get("mst_time", ""),
                        "replay_time": parsed.get("replay_time", ""),
                        "total_time": parsed.get("total_time", ""),
                        "merges": parsed.get("merges", ""),
                        "runtime_wall": f"{wall:.6f}",
                        "error": error,
                    }

                    writer.writerow(row)
                    csvfile.flush()

                    logfile.write(f"{status}, wall={wall:.4f}s\n")

                    if status != "ok":
                        logfile.write(f"    ERROR: {error}\n")

                    logfile.flush()

    print(f"Wrote CSV results to {args.out_csv}")
    print(f"Wrote full log to {args.log}")


if __name__ == "__main__":
    main()
