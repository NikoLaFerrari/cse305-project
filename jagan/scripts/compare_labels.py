import sys
import pandas as pd
import matplotlib.pyplot as plt


def load_data(path, has_header, last_col_label):
    df = pd.read_csv(path) if has_header else pd.read_csv(path, header=None)
    X = df.iloc[:, :-1].to_numpy() if last_col_label else df.to_numpy()
    y = df.iloc[:, -1].to_numpy() if last_col_label else None
    return X, y


def predicted_labels_from_merges(n, merges):
    cluster_points = {i: [i] for i in range(n)}

    for _, row in merges.iterrows():
        a = int(row["cluster_a"])
        b = int(row["cluster_b"])
        c = int(row["new_cluster"])

        cluster_points[c] = cluster_points[a] + cluster_points[b]
        del cluster_points[a]
        del cluster_points[b]

    labels = [0] * n
    for label, points in enumerate(cluster_points.values()):
        for p in points:
            labels[p] = label

    return labels


def main():
    if len(sys.argv) < 6:
        print("Usage: python scripts/compare_labels.py <data.csv> <merges.csv> <has_header> <last_col_label> <output.png>")
        sys.exit(1)

    data_path = sys.argv[1]
    merges_path = sys.argv[2]
    has_header = bool(int(sys.argv[3]))
    last_col_label = bool(int(sys.argv[4]))
    output_path = sys.argv[5]

    X, true_labels = load_data(data_path, has_header, last_col_label)
    merges = pd.read_csv(merges_path)

    pred_labels = predicted_labels_from_merges(len(X), merges)

    fig, axes = plt.subplots(1, 2, figsize=(14, 6))

    if true_labels is not None:
        axes[0].scatter(X[:, 0], X[:, 1], c=true_labels, s=12)
        axes[0].set_title("Ground-truth labels")
    else:
        axes[0].scatter(X[:, 0], X[:, 1], s=12)
        axes[0].set_title("Original data")

    axes[1].scatter(X[:, 0], X[:, 1], c=pred_labels, s=12)
    axes[1].set_title("Single-link HAC result")

    for ax in axes:
        ax.set_xlabel("x")
        ax.set_ylabel("y")

    plt.tight_layout()
    plt.savefig(output_path, dpi=200)
    plt.show()


if __name__ == "__main__":
    main()
