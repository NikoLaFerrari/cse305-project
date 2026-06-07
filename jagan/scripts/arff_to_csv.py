#!/usr/bin/env python3

import sys
import csv
from pathlib import Path

def convert_arff_to_csv(input_path, output_path, max_rows=None):
    attributes = []
    rows = []
    in_data = False

    with open(input_path, "r", encoding="utf-8", errors="ignore") as f:
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
                    continue

                rows.append([x.strip() for x in line.split(",")])

                if max_rows is not None and len(rows) >= max_rows:
                    break

    Path(output_path).parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, "w", newline="") as f:
        writer = csv.writer(f)

        if attributes and rows and len(attributes) == len(rows[0]):
            writer.writerow(attributes)

        writer.writerows(rows)

    print(f"Wrote {len(rows)} rows to {output_path}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: arff_to_csv.py input.arff output.csv [max_rows]")
        sys.exit(1)

    max_rows = int(sys.argv[3]) if len(sys.argv) >= 4 else None
    convert_arff_to_csv(sys.argv[1], sys.argv[2], max_rows)
