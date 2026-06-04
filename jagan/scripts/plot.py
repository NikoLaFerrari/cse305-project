import sys
import pandas as pd
import matplotlib.pyplot as plt


class DSU:
    def __init__(self, n):
        self.parent = list(range(n))

    def find(self, x):
        while self.parent[x] != x:
            self.parent[x] = self.parent[self.parent[x]]
            x = self.parent[x]
        return x

    def union(self, a, b):
        ra = self.find(a)
        rb = self.find(b)
        if ra != rb:
            self.parent[rb] = ra


def main():
    if len(sys.argv) < 5:
        print("Usage: python plot_final_clusters.py <data.csv> <merges.csv> <has_header> <last_col_label>")
        sys.exit(1)

    data_path = sys.argv[1]
    merges_path = sys.argv[2]
    has_header = bool(int(sys.argv[3]))
    last_col_label = bool(int(sys.argv[4]))

    if has_header:
        df = pd.read_csv(data_path)
    else:
        df = pd.read_csv(data_path, header=None)

    if last_col_label:
        X = df.iloc[:, :-1].to_numpy()
    else:
        X = df.to_numpy()

    if X.shape[1] != 2:
        raise ValueError("This visualizer only supports 2D data.")

    merges = pd.read_csv(merges_path)

    n = len(X)
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

    plt.figure(figsize=(7, 7))
    plt.scatter(X[:, 0], X[:, 1], c=labels, s=12)
    plt.title("Final single-link clustering")
    plt.xlabel("x")
    plt.ylabel("y")
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
