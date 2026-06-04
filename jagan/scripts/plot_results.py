import sys
import pandas as pd
import matplotlib.pyplot as plt


def main():
    if len(sys.argv) < 3:
        print("Usage: python scripts/plot_results.py <results.csv> <output_prefix>")
        sys.exit(1)

    path = sys.argv[1]
    prefix = sys.argv[2]

    df = pd.read_csv(path)

    seq = df[df["mode"] == "seq"][["dataset", "total_time"]]
    seq = seq.rename(columns={"total_time": "seq_total"})

    par = df[df["mode"] == "par"].merge(seq, on="dataset")
    par["speedup"] = par["seq_total"] / par["total_time"]

    # Plot total runtime.
    plt.figure(figsize=(10, 6))
    for dataset, group in par.groupby("dataset"):
        group = group.sort_values("threads")
        plt.plot(group["threads"], group["total_time"], marker="o", label=dataset)

    plt.xlabel("Threads")
    plt.ylabel("Total time (seconds)")
    plt.title("Parallel single-link HAC: total runtime")
    plt.legend()
    plt.tight_layout()
    plt.savefig(f"{prefix}_runtime.png", dpi=200)
    plt.close()

    # Plot speedup.
    plt.figure(figsize=(10, 6))
    for dataset, group in par.groupby("dataset"):
        group = group.sort_values("threads")
        plt.plot(group["threads"], group["speedup"], marker="o", label=dataset)

    plt.axhline(1.0, linestyle="--", linewidth=1)
    plt.xlabel("Threads")
    plt.ylabel("Speedup over sequential")
    plt.title("Parallel single-link HAC: speedup")
    plt.legend()
    plt.tight_layout()
    plt.savefig(f"{prefix}_speedup.png", dpi=200)
    plt.close()

    # Stage breakdown for sequential.
    seq_rows = df[df["mode"] == "seq"].copy()
    seq_rows = seq_rows.set_index("dataset")

    stage_cols = ["edge_time", "sort_time", "kruskal_time"]

    seq_rows[stage_cols].plot(
        kind="bar",
        stacked=True,
        figsize=(10, 6)
    )

    plt.ylabel("Time (seconds)")
    plt.title("Sequential runtime breakdown")
    plt.tight_layout()
    plt.savefig(f"{prefix}_seq_breakdown.png", dpi=200)
    plt.close()

    print(f"Saved:")
    print(f"  {prefix}_runtime.png")
    print(f"  {prefix}_speedup.png")
    print(f"  {prefix}_seq_breakdown.png")


if __name__ == "__main__":
    main()
