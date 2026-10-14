# Design Space Exploration Report: carry_select_adder_8bit

## Circuit Information

| Parameter | Value |
| :--- | :--- |
| Circuit Name | `carry_select_adder_8bit` |
| Primary Inputs (PIs) | 17 |
| Primary Outputs (POs) | 9 |
| Logic Gates | 71 |
| Total Circuit Nodes | 196 |
| Original Total Faults | 374 |
| Collapsed Faults | 294 (21.4% reduction) |
| Test Vectors Evaluated | 2048 |

## Algorithm Performance Comparison

| Algorithm | Time (ms) | Speedup vs SFS | Fault Coverage (%) | Undetected Faults | Compacted Test Vectors |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Serial Fault Simulation (SFS)** | 7.616 | 1.00x | 99.66% | 1 | 26 (98.7% red.) |
| **Parallel Pattern Fault Simulation (PPFS)** | 1.415 | 5.38x | 99.66% | 1 | 20 (99.0% red.) |
| **Concurrent Fault Simulation (CFS)** | 9.823 | 0.78x | 99.66% | 1 | 26 (98.7% red.) |

## Undetected Faults

The following 1 faults remained undetected:

- `nc3_br5 SA1`

## Vector Budget Scaling (Design Space Exploration)

| Vectors | SFS Cov (%) | SFS Time (ms) | PPFS Cov (%) | PPFS Time (ms) | CFS Cov (%) | CFS Time (ms) |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 2 | 49.7% | 0.603 | 49.7% | 0.437 | 49.7% | 0.100 |
| 4 | 67.7% | 0.747 | 67.7% | 0.450 | 67.7% | 0.145 |
| 8 | 82.3% | 0.952 | 82.3% | 0.460 | 82.3% | 0.196 |
| 16 | 94.6% | 1.220 | 94.6% | 0.474 | 94.6% | 0.269 |
| 32 | 98.3% | 1.406 | 98.3% | 0.521 | 98.3% | 0.359 |
| 64 | 99.0% | 1.531 | 99.0% | 0.518 | 99.0% | 0.562 |
| 128 | 99.7% | 1.896 | 99.7% | 0.589 | 99.7% | 0.846 |
| 256 | 99.7% | 2.259 | 99.7% | 0.563 | 99.7% | 1.381 |
| 512 | 99.7% | 2.983 | 99.7% | 0.669 | 99.7% | 2.608 |
| 1024 | 99.7% | 4.607 | 99.7% | 0.856 | 99.7% | 4.953 |
| 2048 | 99.7% | 7.695 | 99.7% | 1.167 | 99.7% | 9.858 |
