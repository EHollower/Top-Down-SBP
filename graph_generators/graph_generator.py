import numpy as np
import math
import matplotlib.pyplot as plt
from collections import defaultdict
from typing import List, Any


def write_graph_to_file(file : str, graph : List[List[Any]], assignment : list):
    n = len(graph)
    with open(file, "w") as f:
        f.write(f'{n}\n')
        f.write(f'{graph}\n')
        f.write(f'{assignment}\n')

def generate_graph(num_vertices : int, num_clusters : int, file : str = "graph.txt"):
    G = [[] for _ in range(num_vertices)]
    assignment = [np.random.randint(0, num_clusters) for _ in range(num_vertices)]
    
    prob_of_internal_edge = 0.85
    prob_of_inter_cluster_edge = 0.05
    
    for i in range(0, num_vertices):
        for j in range(i, num_vertices):
            if assignment[i] == assignment[j]:
                if np.random.random() < prob_of_internal_edge:
                    G[i].append(j)
            else:
                if np.random.random() < prob_of_inter_cluster_edge:
                    G[i].append(j)
    
    print(G)
    write_graph_to_file(file, G, assignment)
    
    n = len(G)

    # ---- group nodes by cluster ----
    clusters = defaultdict(list)
    for node, c in enumerate(assignment):
        clusters[c].append(node)
    
    num_clusters = len(clusters)
    
    # ---- choose cluster centers (big circle) ----
    cluster_radius = 4.0
    cluster_angles = [
        2 * math.pi * i / num_clusters for i in range(num_clusters)
    ]
    
    cluster_centers = {}
    for angle, c in zip(cluster_angles, clusters.keys()):
        cluster_centers[c] = (
            cluster_radius * math.cos(angle),
            cluster_radius * math.sin(angle)
        )
    
    # ---- place nodes around each cluster center ----
    pos = {}  # node -> (x, y)
    node_radius = 1.0
    
    for c, nodes in clusters.items():
        k = len(nodes)
        for i, node in enumerate(nodes):
            angle = 2 * math.pi * i / k
            cx, cy = cluster_centers[c]
            pos[node] = (
                cx + node_radius * math.cos(angle),
                cy + node_radius * math.sin(angle)
            )
    
    # ---- color map for clusters ----
    colors = plt.cm.tab10(range(num_clusters))
    cluster_color = {
        c: colors[i % len(colors)]
        for i, c in enumerate(clusters.keys())
    }
    
    # ---- draw edges ----
    for u in range(n):
        for v in G[u]:
            x = [pos[u][0], pos[v][0]]
            y = [pos[u][1], pos[v][1]]
            plt.plot(x, y, color="gray", alpha=0.5, zorder=1)
    
    # ---- draw nodes ----
    for node, (x, y) in pos.items():
        c = assignment[node]
        plt.scatter(
            x, y,
            s=300,
            color=cluster_color[c],
            edgecolors="black",
            zorder=2
        )
        plt.text(x, y, str(node), ha="center", va="center", color="white")
    
    plt.axis("off")
    plt.show()
    



generate_graph(30, 4)




