# Design Space Exploration Report: alu_6bit_gate_level

## Circuit Information

| Parameter | Value |
| :--- | :--- |
| Circuit Name | `alu_6bit_gate_level` |
| Primary Inputs (PIs) | 15 |
| Primary Outputs (POs) | 7 |
| Logic Gates | 80 |
| Total Circuit Nodes | 236 |
| Original Total Faults | 458 |
| Collapsed Faults | 328 (28.4% reduction) |
| Test Vectors Evaluated | 32768 |

## Algorithm Performance Comparison

| Algorithm | Time (ms) | Speedup vs SFS | Fault Coverage (%) | Undetected Faults | Compacted Test Vectors |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Serial Fault Simulation (SFS)** | 429.062 | 1.00x | 98.17% | 6 | 88 (99.7% red.) |
| **Parallel Pattern Fault Simulation (PPFS)** | 24.184 | 17.74x | 98.17% | 6 | 74 (99.8% red.) |
| **Concurrent Fault Simulation (CFS)** | 171.876 | 2.50x | 98.17% | 6 | 88 (99.7% red.) |

## Undetected Faults

The following 6 faults remained undetected:

- `s0_br3 SA1`
- `s0_br5 SA1`
- `s0_br7 SA1`
- `s0_br9 SA1`
- `s0_br11 SA1`
- `s0_br13 SA1`

## Vector Budget Scaling (Design Space Exploration)

| Vectors | SFS Cov (%) | SFS Time (ms) | PPFS Cov (%) | PPFS Time (ms) | CFS Cov (%) | CFS Time (ms) |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 2 | 18.6% | 0.540 | 18.6% | 0.538 | 18.6% | 0.055 |
| 4 | 27.7% | 0.896 | 27.7% | 0.644 | 27.7% | 0.095 |
| 8 | 32.3% | 1.441 | 32.3% | 0.554 | 32.3% | 0.125 |
| 16 | 39.3% | 2.263 | 39.3% | 0.534 | 39.3% | 0.184 |
| 32 | 46.3% | 3.892 | 46.3% | 0.556 | 46.3% | 0.331 |
| 64 | 53.4% | 6.313 | 53.4% | 0.593 | 53.4% | 0.529 |
| 128 | 60.4% | 10.592 | 60.4% | 0.817 | 60.4% | 0.920 |
| 256 | 67.4% | 17.184 | 67.4% | 1.213 | 67.4% | 1.815 |
| 512 | 72.6% | 27.014 | 72.6% | 1.897 | 72.6% | 3.094 |
| 1024 | 76.8% | 43.804 | 76.8% | 2.969 | 76.8% | 6.014 |
| 2048 | 81.1% | 71.926 | 81.1% | 4.888 | 81.1% | 11.423 |
| 4096 | 85.4% | 118.607 | 85.4% | 7.774 | 85.4% | 40.093 |
| 8192 | 89.6% | 190.664 | 89.6% | 12.069 | 89.6% | 44.006 |
| 16384 | 93.9% | 296.641 | 93.9% | 18.088 | 93.9% | 86.246 |
| 32768 | 98.2% | 427.444 | 98.2% | 23.893 | 98.2% | 180.240 |
