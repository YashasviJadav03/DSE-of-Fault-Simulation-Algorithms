# Design Space Exploration Report: comparatOR_4bit_structural

## Circuit Information

| Parameter | Value |
| :--- | :--- |
| Circuit Name | `comparatOR_4bit_structural` |
| Primary Inputs (PIs) | 8 |
| Primary Outputs (POs) | 3 |
| Logic Gates | 23 |
| Total Circuit Nodes | 73 |
| Original Total Faults | 140 |
| Collapsed Faults | 98 (30.0% reduction) |
| Test Vectors Evaluated | 256 |

## Algorithm Performance Comparison

| Algorithm | Time (ms) | Speedup vs SFS | Fault Coverage (%) | Undetected Faults | Compacted Test Vectors |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Serial Fault Simulation (SFS)** | 0.866 | 1.00x | 100.00% | 0 | 25 (90.2% red.) |
| **Parallel Pattern Fault Simulation (PPFS)** | 0.191 | 4.53x | 100.00% | 0 | 21 (91.8% red.) |
| **Concurrent Fault Simulation (CFS)** | 0.605 | 1.43x | 100.00% | 0 | 25 (90.2% red.) |

## Undetected Faults

*All simulated faults were detected (100% coverage).*

## Vector Budget Scaling (Design Space Exploration)

| Vectors | SFS Cov (%) | SFS Time (ms) | PPFS Cov (%) | PPFS Time (ms) | CFS Cov (%) | CFS Time (ms) |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 2 | 37.8% | 0.067 | 37.8% | 0.048 | 37.8% | 0.029 |
| 4 | 44.9% | 0.087 | 44.9% | 0.046 | 44.9% | 0.034 |
| 8 | 52.0% | 0.130 | 52.0% | 0.048 | 52.0% | 0.048 |
| 16 | 59.2% | 0.206 | 59.2% | 0.048 | 59.2% | 0.074 |
| 32 | 69.4% | 0.320 | 69.4% | 0.053 | 69.4% | 0.118 |
| 64 | 79.6% | 0.466 | 79.6% | 0.057 | 79.6% | 0.188 |
| 128 | 89.8% | 0.628 | 89.8% | 0.075 | 89.8% | 0.301 |
| 256 | 100.0% | 0.796 | 100.0% | 0.104 | 100.0% | 0.547 |
