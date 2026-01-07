#!/bin/bash
# Quick CSV analysis using awk

CSV_FILE="results/benchmark_results.csv"

if [ ! -f "$CSV_FILE" ]; then
    echo "Error: $CSV_FILE not found!"
    echo "Run ./bin/sbp_benchmark first to generate results."
    exit 1
fi

echo "========================================="
echo "  SBP Benchmark Results Summary"
echo "========================================="
echo ""

echo "=== Average Runtime by Algorithm ==="
awk -F',' 'NR>1 {sum[$5]+=$7; count[$5]++} 
    END {for (alg in sum) printf "%s: %.4f seconds\n", alg, sum[alg]/count[alg]}' \
    $CSV_FILE | sort
echo ""

echo "=== Average NMI (Accuracy) by Algorithm ==="
awk -F',' 'NR>1 {sum[$5]+=$9; count[$5]++} 
    END {for (alg in sum) printf "%s: %.4f (%.1f%%)\n", alg, sum[alg]/count[alg], 100*sum[alg]/count[alg]}' \
    $CSV_FILE | sort
echo ""

echo "=== Average Memory Usage by Algorithm ==="
awk -F',' 'NR>1 {sum[$5]+=$8; count[$5]++} 
    END {for (alg in sum) printf "%s: %.0f MB\n", alg, sum[alg]/count[alg]}' \
    $CSV_FILE | sort
echo ""

echo "=== Performance by Graph Size ==="
echo "Graph | N    | Algorithm | Runtime (s) | NMI    | Memory (MB)"
echo "------|------|-----------|-------------|--------|------------"
awk -F',' 'NR>1 {
    key=$1 "_" $2 "_" $5;
    sum_rt[key] += $7; sum_nmi[key] += $9; sum_mem[key] += $8; count[key]++;
    graph[$1] = $2; alg[key] = $5;
}
END {
    for (k in sum_rt) {
        split(k, parts, "_");
        gid = parts[1]; n = parts[2]; algorithm = parts[3];
        printf "%5d | %4d | %-9s | %11.4f | %.4f | %10.0f\n", 
               gid, n, algorithm, sum_rt[k]/count[k], sum_nmi[k]/count[k], sum_mem[k]/count[k];
    }
}' $CSV_FILE | sort -n
echo ""

echo "=== Speedup: Top-Down vs. Bottom-Up ==="
awk -F',' 'NR>1 {
    key = $1 "_" $2;  # graph_id and num_vertices
    if ($5 == "TopDown") td_rt[key] += $7;
    if ($5 == "BottomUp") bu_rt[key] += $7;
    count[key]++;
    n[key] = $2;
}
END {
    print "N    | Top-Down (s) | Bottom-Up (s) | Speedup";
    print "-----|--------------|---------------|--------";
    for (k in td_rt) {
        avg_td = td_rt[k] / (count[k]/2);
        avg_bu = bu_rt[k] / (count[k]/2);
        speedup = avg_bu / avg_td;
        printf "%4d | %12.4f | %13.4f | %6.1fx\n", n[k], avg_td, avg_bu, speedup;
    }
}' $CSV_FILE | sort -n
echo ""

echo "========================================="
echo "Full results in: $CSV_FILE"
echo "========================================="
