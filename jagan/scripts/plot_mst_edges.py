import sys
import pandas as pd
import matplotlib.pyplot as plt


def main():
    if len(sys.argv) < 6:
        print(
            "Usage: python scripts/plot_mst_edges.py "
            "<data.csv> <merges.csv> <has_header> <last_col_label> <output.png>"
        )
        sys.exit(1)

    data_path = sys.argv[1]
    merges_path = sys.argv[2]
    has_header = bool(int(sys.argv[3]))
    last_col_label = bool(int(sys.argv[4]))
    output_path = sys.argv[5]

    df = pd.read_csv(data_path) if has_header else pd.read_csv(data_path, header=None)

    if last_col_label:
        X = df.iloc[:, :-1].to_numpy()
        labels = df.iloc[:, -1].to_numpy()
    else:
        X = df.to_numpy()
        labels = None

    if X.shape[1] != 2:
        raise ValueError("This visualizer only supports 2D datasets.")

    merges = pd.read_csv(merges_path)

    required = {"edge_u", "edge_v"}
    if not required.issubset(set(merges.columns)):
        raise ValueError("Merge file must contain edge_u and edge_v columns.")

    plt.figure(figsize=(8, 8))

    for _, row in merges.iterrows():
        u = int(row["edge_u"])
        v = int(row["edge_v"])

        plt.plot(
            [X[u, 0], X[v, 0]],
            [X[u, 1], X[v, 1]],
            linewidth=0.55,
            alpha=0.40,
            zorder=1,
        )

    if labels is not None:
        plt.scatter(X[:, 0], X[:, 1], c=labels, s=12, zorder=2)
    else:
        plt.scatter(X[:, 0], X[:, 1], s=12, zorder=2)

    plt.title("Accepted Kruskal edges / Single-link MST structure")
    plt.xlabel("x")
    plt.ylabel("y")
    plt.tight_layout()
    plt.savefig(output_path, dpi=200)
    plt.show()


if __name__ == "__main__":
    main()
