Task List

K: Research software algorithms to implement.
K: Develop the CPU-only software implementation in C++ 
K: Implement the test vectors and datasets for validation and performance measurement.
K: Measure the baseline latency, throughput, and CPU usage of the C++ software on the ARM processor
K: Analyze the C++ software to identify the bottleneck for hardware migration.

F: Design the core architecture of the hardware accelerator
D: Validate the generated hardware IP core using a simulation testbench (Python's cocotb)
F: Create the Vivado block diagram to integrate the custom hardware IP core with the ARM processor and memory system.
F: Define the AXI-Lite interface and register map for sending control signals and retrieving status to/from the accelerator
F: Configure AXI interfaces for efficient data transfer between memory and the hardware accelerator
F: Document the memory and I/O mapping required for the host C++ application to communicate with the hardware IP
D: Research design patterns for integrating the ARM software driver with the generated AXI-Lite hardware IP core registers.

F: Synthesize, place, and route the complete integrated hardware design onto the Zynq FPGA fabric
F: Verify the generation of the bitstream and load the hardware configuration onto the Zynq board.
F: Develop the host C++ application for the ARM processor that calls the hardware accelerator and handles data buffer transfers
F: Refine the host application's data handling to ensure optimal data transfer rates across the AXI buses.

D: Test the end-to-end functionality of the hardware-accelerated system against the software baseline.
D: Measure latency and throughput metrics of the hardware-accelerated version running on the Zynq board.
D: Determine power consumption of the Zynq board during the execution of both the baseline and accelerated tests
D: Create visuals to present the quantifiable results of the hardware acceleration.

F - Fiona
D - Danni
K - Kelly

