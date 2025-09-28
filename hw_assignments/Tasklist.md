
K: 1. Research software algorithms to implement.
K: 2. Develop the CPU-only software implementation in C++ 
K: 3. Implement the test vectors and datasets for validation and performance measurement.
K: 4. Measure the baseline latency, throughput, and CPU usage of the C++ software on the ARM processor
K: 5. Analyze the C++ software to identify the bottleneck for hardware migration.

F: 6. Design the core architecture of the hardware accelerator
F: 7. Validate the generated hardware IP core using a simulation testbench (Python's cocotb)
F: 8. Create the Vivado block diagram to integrate the custom hardware IP core with the ARM processor and memory system.
F: 9. Define the AXI-Lite interface and register map for sending control signals and retrieving status to/from the accelerator
F: 10. Configure AXI interfaces for efficient data transfer between memory and the hardware accelerator
F: 11. Document the memory and I/O mapping required for the host C++ application to communicate with the hardware IP

F: 12. Synthesize, place, and route the complete integrated hardware design onto the Zynq FPGA fabric
F: 13. Verify the generation of the bitstream and load the hardware configuration onto the Zynq board.
F: 14. Develop the host C++ application for the ARM processor that calls the hardware accelerator and handles data buffer transfers
F: 15. Refine the host application's data handling to ensure optimal data transfer rates across the AXI buses.

D: 16. Test the end-to-end functionality of the hardware-accelerated system against the software baseline.
D: 17. Measure latency and throughput metrics of the hardware-accelerated version running on the Zynq board.
D: 18. Determine power consumption of the Zynq board during the execution of both the baseline and accelerated tests
D: 19. Create visuals to present the quantifiable results of the hardware acceleration.




