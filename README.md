# BMP-Image-Pipeline-with-SystemC-AMS-Co-Simulation

This repository presents a comprehensive BMP image processing pipeline designed with SystemC-AMS co-simulation techniques. The pipeline models a CMOS image sensor using the SystemC-AMS Transaction-Level Modeling (TLM) and Timed Data Flow (TDF) approaches, including an analog-to-digital conversion stage and optional pre-mapping via a one-dimensional Lookup Table (LUT). The sensor produces a discrete-event stream for further digital image processing.

The pipeline integrates a Verilated Canny edge detection module for image signal processing (ISP), driven by a SystemC register driver. A post-LUT processing stage computes histograms for each frame and generates CSV files for data analysis and validation purposes.

To optimize data handling, pixel data are packed into 256-bit beats with a custom BurstPacker module. The pipeline interfaces with a simple LPDDR memory model that supports burst write/read operations, with performance reaching approximately 3.2 GB/s read throughput at 100 MHz under bus-limited conditions.

Additionally, a command-line interface (CLI) offers configurable flags for gamma correction, gain, offset, and external LUT usage. These options allow deterministic traffic generation and detailed performance reporting to facilitate system-on-chip (SoC) level simulations and design evaluations.
