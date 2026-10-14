# Design Space Exploration Report: barrel_shifter_8bit

## Circuit Information

| Parameter | Value |
| :--- | :--- |
| Circuit Name | `barrel_shifter_8bit` |
| Primary Inputs (PIs) | 11 |
| Primary Outputs (POs) | 8 |
| Logic Gates | 75 |
| Total Circuit Nodes | 179 |
| Original Total Faults | 342 |
| Collapsed Faults | 229 (33.0% reduction) |
| Test Vectors Evaluated | 2048 |

## Algorithm Performance Comparison

| Algorithm | Time (ms) | Speedup vs SFS | Fault Coverage (%) | Undetected Faults | Compacted Test Vectors |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Serial Fault Simulation (SFS)** | 33.423 | 1.00x | 90.83% | 21 | 36 (98.2% red.) |
| **Parallel Pattern Fault Simulation (PPFS)** | 2.372 | 14.09x | 90.83% | 21 | 32 (98.4% red.) |
| **Concurrent Fault Simulation (CFS)** | 9.170 | 3.64x | 90.83% | 21 | 36 (98.2% red.) |

## Undetected Faults

The following 21 faults remained undetected:

- `m0_7_b SA0`
- `m1_7_b SA0`
- `m1_6_b SA0`
- `m2_7_b SA0`
- `m2_6_b SA0`
- `m2_5_b SA0`
- `m2_4_b SA0`
- `s2_br2 SA0`
- `s2_br2 SA1`
- `s2_br3 SA0`
- `s2_br3 SA1`
- `s2_br4 SA0`
- `s2_br4 SA1`
- `s2_br5 SA0`
- `s2_br5 SA1`
- `s1_br2 SA0`
- `s1_br2 SA1`
- `s1_br3 SA0`
- `s1_br3 SA1`
- `s0_br2 SA0`
- `s0_br2 SA1`

## Vector Budget Scaling (Design Space Exploration)

| Vectors | SFS Cov (%) | SFS Time (ms) | PPFS Cov (%) | PPFS Time (ms) | CFS Cov (%) | CFS Time (ms) |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 2 | 24.5% | 0.279 | 24.5% | 0.274 | 24.5% | 0.042 |
| 4 | 27.1% | 0.395 | 27.1% | 0.282 | 27.1% | 0.055 |
| 8 | 28.8% | 0.633 | 28.8% | 0.279 | 28.8% | 0.084 |
| 16 | 38.0% | 1.023 | 38.0% | 0.291 | 38.0% | 0.154 |
| 32 | 43.7% | 1.755 | 43.7% | 0.297 | 43.7% | 0.232 |
| 64 | 50.7% | 2.721 | 50.7% | 0.294 | 50.7% | 0.358 |
| 128 | 57.6% | 4.657 | 57.6% | 0.460 | 57.6% | 0.678 |
| 256 | 65.9% | 8.175 | 65.9% | 0.716 | 65.9% | 1.233 |
| 512 | 74.2% | 13.351 | 74.2% | 1.010 | 74.2% | 2.361 |
| 1024 | 82.5% | 21.461 | 82.5% | 1.529 | 82.5% | 4.647 |
| 2048 | 90.8% | 33.032 | 90.8% | 2.143 | 90.8% | 9.363 |
