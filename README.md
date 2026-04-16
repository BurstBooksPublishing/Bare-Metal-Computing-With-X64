# Bare Metal Computing With X64

### Cover
<img src="covers/Front3.png" alt="Book Cover" width="300" style="max-width: 100%; height: auto; border-radius: 6px; box-shadow: 0 3px 8px rgba(0,0,0,0.1);"/>

### Repository Structure
- `covers/`: Book cover images
- `blurbs/`: Promotional blurbs
- `infographics/`: Marketing visuals
- `source_code/`: Code samples
- `manuscript/`: Drafts and format.txt for TOC
- `marketing/`: Ads and press releases
- `additional_resources/`: Extras

View the live site at [burstbookspublishing.github.io/bare-metal-computing-with-x64](https://burstbookspublishing.github.io/bare-metal-computing-with-x64/)
---

- Bare Metal Computing With X64
- Low Level Coding of Native Instructions

---
## Chapter 1. The Modern CPU Execution Model
### Section 1. Instruction Set Architecture and Machine Code
- Opcode formats and binary encoding structure
- Register files and architectural state
- Addressing modes and operand resolution
- Flags and condition codes
- Instruction decoding and execution flow
- Control registers and system state

### Section 2. Execution Modes and Privilege Architecture
- Real mode fundamentals
- Protected mode structure
- Long mode (64-bit) operation
- Ring levels and privilege enforcement
- Transitioning between modes
- Model-specific registers and extended state

### Section 3. Microarchitectural Realities
- Pipelines and superscalar execution
- Out-of-order execution mechanics
- Branch prediction and speculation
- Cache hierarchies and coherency protocols
- Translation lookaside buffers (TLBs)
- Performance monitoring counters

---
## Chapter 2. Boot and Control Transfer on Modern Hardware
### Section 1. Firmware Foundations
- BIOS legacy boot process
- UEFI boot architecture
- Secure Boot and its implications
- Firmware memory maps
- Power-on reset state
- POST and hardware initialization

### Section 2. Bootloader Architecture
- Boot sector structure and constraints
- Stage 1 and Stage 2 loaders
- Loading binaries from storage
- Stack setup and early execution environment
- Enabling A20 and memory access above 1MB
- Transition to protected and long mode

### Section 3. Establishing Full Control
- Exiting firmware services
- Identity mapping and early paging
- Setting up GDT and IDT
- Enabling long mode execution
- Control register configuration
- Verifying CPU features and capabilities

---
## Chapter 3. Memory Architecture and Deterministic Control
### Section 1. Physical and Virtual Memory Models
- Physical address space layout
- Virtual address translation mechanisms
- Page table hierarchies (PML4 and beyond)
- Page attributes and access control
- Large pages and huge pages
- Memory type range registers (MTRRs)

### Section 2. Paging and Execution Management
- Identity mapping strategies
- Higher-half kernel layouts
- Disabling or simplifying paging
- Managing CR3 and TLB behavior
- Cache control and memory ordering
- Executable vs non-executable regions

### Section 3. Deterministic Memory Operation
- Eliminating unpredictable interrupts
- Cache behavior and false sharing
- Interrupt masking and NMI considerations
- Precise timing with TSC
- DMA and IOMMU configuration
- Memory barriers and ordering guarantees

---
## Chapter 4. Interrupts, Exceptions, and Hardware Control
### Section 1. Interrupt Architecture and Descriptor Tables
- Interrupt Descriptor Table (IDT) structure
- Gate types and privilege transitions
- Interrupt vector layout
- Interrupt stack tables (IST)
- Task State Segment (TSS) configuration
- Loading and activating the IDT

### Section 2. Exception Handling and Fault Management
- Processor-defined exception types
- Faults, traps, and aborts
- Error codes and stack frames
- Double faults and triple faults
- Recoverable vs non-recoverable conditions
- Designing stable exception handlers

### Section 3. Advanced Interrupt Control
- Legacy PIC vs Local APIC
- I/O APIC routing and configuration
- Inter-Processor Interrupts (IPI)
- Non-Maskable Interrupts (NMI)
- Interrupt prioritization and nesting
- Deterministic interrupt masking strategies

---
## Chapter 5. Direct Hardware Access and Bus-Level Control
### Section 1. Memory-Mapped and Port-Mapped I/O
- Memory-mapped I/O fundamentals
- Port I/O instructions and usage
- Access width and alignment constraints
- Volatile semantics and ordering
- Device register interaction models
- Side effects of hardware access

### Section 2. PCI and Device Enumeration
- PCI configuration space structure
- Bus, device, and function addressing
- Base Address Registers (BARs)
- Capability lists and extended capabilities
- Enumerating and initializing devices
- Mapping device memory regions

### Section 3. Direct Memory Access and IOMMU
- DMA fundamentals and descriptor rings
- Bus mastering configuration
- Cache coherency with DMA
- IOMMU mapping and isolation
- Scatter-gather transfers
- Ensuring deterministic device interaction

---
## Chapter 6. Timing, Scheduling, and Execution Control
### Section 1. Hardware Timers and Timekeeping
- Programmable Interval Timer (PIT)
- Local APIC timer
- High Precision Event Timer (HPET)
- Time Stamp Counter (TSC)
- Invariant TSC and frequency scaling
- Clock calibration techniques

### Section 2. Scheduling Without an Operating System
- Cooperative scheduling models
- Preemptive scheduling design
- Context switching mechanics
- Saving and restoring CPU state
- Stack management for multiple tasks
- Minimal task control structures

### Section 3. Deterministic Execution Guarantees
- Interrupt latency measurement
- Jitter analysis and reduction
- Cache warming strategies
- Lock-free synchronization
- Spinlocks and atomic primitives
- Real-time deadline enforcement

---
## Chapter 7. AI as Machine Code Author
### Section 1. Human–AI Architectural Workflow
- Defining hardware-level specifications
- Declaring architectural constraints
- Iterative refinement of execution models
- Prompt structures for deterministic code generation
- Validating AI understanding of CPU state
- Maintaining architectural invariants

### Section 2. Machine Code Synthesis
- Translating requirements into opcode sequences
- Register allocation strategies
- Stack discipline enforcement
- Control flow construction at binary level
- Direct encoding of instruction bytes
- Mode-aware instruction generation

### Section 3. Validation and Semantic Assurance
- Binary inspection and disassembly verification
- Control flow graph reconstruction
- Detecting undefined behavior
- Ensuring privilege correctness
- Verifying memory safety constraints
- Performance profiling at instruction granularity

---
## Chapter 8. Debugging and Verification Without an OS
### Section 1. Hardware-Level Debugging Tools
- Serial console diagnostics
- JTAG and hardware debugging interfaces
- Using QEMU and hardware emulators
- GDB remote debugging
- Breakpoints and watchpoints at machine level
- Single-stepping instruction execution

### Section 2. Fault Analysis and Recovery
- Stack frame reconstruction
- Register state inspection
- Handling page faults and general protection faults
- Diagnosing triple faults
- Memory corruption detection
- Structured crash reporting

### Section 3. Deterministic Testing and Replay
- Building reproducible test harnesses
- Hardware simulation environments
- Deterministic input injection
- Fault injection testing
- Measuring execution timing precisely
- Regression testing for binary code

---
## Chapter 9. Security and Containment in Bare-Metal Systems
### Section 1. Controlled Hardware Exposure
- Limiting device access regions
- Configuring IOMMU isolation
- Restricting DMA channels
- Interrupt masking boundaries
- Hardware watchdog timers
- Deadman switch mechanisms

### Section 2. Memory Protection Strategies
- Selective paging enforcement
- Executable vs non-executable segmentation
- Capability-based memory models
- Guard pages and stack canaries
- Runtime invariant checking
- Controlled privilege escalation

### Section 3. Fail-Safe and Recovery Design
- Rollback execution strategies
- Snapshotting CPU and memory state
- Reset vectors and safe reboot paths
- Hardware kill-switch design
- Isolation of experimental execution regions
- Designing for predictable failure modes

---
## Chapter 10. Real-Time Robotics and Control Systems
### Section 1. Deterministic Control Loops
- Servo motor control timing
- Sensor polling and interrupt-driven acquisition
- Closed-loop PID implementation in machine code
- Fixed-point vs floating-point tradeoffs
- Latency budgeting and jitter constraints
- Coordinated multi-axis motion control

### Section 2. Sensor Fusion and Data Processing
- IMU integration and filtering
- Kalman filter execution determinism
- ADC and DAC hardware interaction
- Interrupt-priority design for sensor inputs
- High-rate data buffering strategies
- Fail-safe detection and safe-stop logic

### Section 3. Hardware Isolation and Safety
- Watchdog integration
- Redundant control channels
- Deterministic scheduling for safety compliance
- Emergency interrupt handling
- Power-loss recovery strategies
- Physical actuator isolation mechanisms

---
## Chapter 11. Ultra-Low-Latency Systems and Financial Engines
### Section 1. Deterministic Network Processing
- Direct NIC programming
- Descriptor ring management
- Interrupt moderation tuning
- Zero-copy packet handling
- Kernel bypass architecture
- Tick-to-trade timing measurement

### Section 2. Microsecond-Level Optimization
- Cache-aware data layout
- Branch prediction alignment
- Eliminating pipeline stalls
- Lock-free queue implementation
- NUMA considerations
- Instruction-level performance profiling

### Section 3. FPGA and Accelerator Integration
- PCIe accelerator coordination
- Shared memory buffers
- DMA synchronization
- Deterministic co-processing models
- Latency auditing and compliance logging
- Hardware timestamping strategies

---
## Chapter 12. Signal Processing and Edge AI Appliances
### Section 1. Software-Defined Radio and DSP
- Baseband processing pipelines
- SIMD vector instruction utilization
- Real-time FFT implementation
- Buffer management for continuous streams
- Interrupt-driven sampling
- Jitter minimization techniques

### Section 2. Bare-Metal AI Inference
- Neural network execution without OS
- Direct accelerator invocation
- Memory-mapped model loading
- Batch vs streaming inference models
- Deterministic tensor scheduling
- Thermal and power control considerations

### Section 3. Edge Deployment Architecture
- Minimal boot-time inference startup
- Secure model update mechanisms
- Hardware isolation for inference engines
- Remote telemetry without OS overhead
- Low-footprint distributed inference nodes
- Failover and redundancy design

---
## Chapter 13. Bare-Metal Distributed Compute Systems
### Section 1. Custom Interconnect Architectures
- Direct Ethernet programming
- Lightweight message framing protocols
- Zero-copy transmission strategies
- Deterministic packet scheduling
- Clock synchronization across nodes
- Latency measurement and compensation

### Section 2. Message Passing Without an OS
- Ring buffers for inter-node communication
- Reliable transport over raw frames
- Collective operations (broadcast, reduce, scatter)
- Flow control mechanisms
- Deadlock avoidance strategies
- Remote Direct Memory Access (RDMA) concepts

### Section 3. Fault Tolerance and Recovery
- Heartbeat monitoring
- Deterministic state replication
- Checkpointing memory and register state
- Node failure detection
- Consensus without full OS stacks
- Safe rejoin and cluster stabilization

---
## Chapter 14. Experimental Hardware and ISA Research Platforms
### Section 1. Custom Instruction Set Design
- Defining new opcode encodings
- Extending existing ISAs
- Designing control register semantics
- Memory model experimentation
- Encoding formats and decode logic
- Compatibility considerations

### Section 2. FPGA-Based CPU Prototyping
- Softcore CPU implementation
- Pipeline design experimentation
- Cache and memory controller design
- Timing closure challenges
- Hardware/software co-design workflow
- Instrumentation for microarchitectural analysis

### Section 3. Performance and Security Experimentation
- Custom performance counter design
- Microarchitectural side-channel testing
- Novel isolation mechanisms
- Capability-based execution experiments
- Hardware-level invariant enforcement
- Research benchmarking frameworks

---
## Chapter 15. Designing AI-Native Hardware Architectures
### Section 1. Reconfigurable Compute Fabrics
- CPU-FPGA hybrid integration
- Dynamic hardware partitioning
- Runtime reconfiguration models
- Hardware acceleration synthesis from AI
- Managing power-performance tradeoffs
- Partial reconfiguration safety models

### Section 2. Capability-Based and Tagged Architectures
- Hardware-enforced access tokens
- Tagged memory fundamentals
- Bounds enforcement in silicon
- Temporal safety mechanisms
- Delegation and revocation models
- Integration with machine code generation

### Section 3. The Post-Operating System Computer
- Direct application-to-hardware mapping
- Minimal execution substrates
- Eliminating traditional software layers
- Hardware-supervised execution domains
- Security without a monolithic kernel
- Toolchain evolution for AI-native systems
- Appendix A. Minimal Boot and Long Mode Reference

### Section 1. Reset and Firmware Handoff State
- CPU state at power-on
- Real mode defaults
- Firmware-provided memory map
- Register assumptions at control transfer

### Section 2. Transition to 64-bit Execution
- Required control register configuration
- Minimal GDT layout requirements
- Paging enable sequence
- Long mode activation order
- Verifying successful mode switch

### Section 3. Mode-Switch Failure Diagnostics
- Triple fault root causes
- Descriptor misconfiguration symptoms
- Paging structure errors
- Debugging early boot hangs
- Appendix B. Bare-Metal Debugging Patterns

### Section 1. Serial Console Debug Pattern
- Minimal COM port initialization
- Early boot diagnostic output
- Structured debug logging format
- Handling output during faults

### Section 2. Emulator and Hardware Debug Pattern
- QEMU execution model
- Single-step debugging workflow
- Breakpoint and watchpoint usage
- Register and memory inspection techniques

### Section 3. Fault Capture and Recovery Templates
- Register dump format
- Stack frame reconstruction
- Page fault decoding workflow
- Structured crash reporting format
- Appendix C. Deterministic Execution Reference

### Section 1. Precise Timing Measurement
- TSC usage patterns
- Interrupt latency measurement
- Calibration strategies
- Detecting frequency drift

### Section 2. Interrupt and Concurrency Control
- Interrupt masking strategies
- Atomic operation patterns
- Memory barrier usage
- Spinlock design templates

### Section 3. Jitter Reduction Techniques
- Cache warming strategies
- Eliminating unintended interrupts
- Avoiding false sharing
- Deterministic scheduling patterns
- Appendix D. AI–Machine Code Engineering Workflow

### Section 1. Constraint Declaration Framework
- Defining architectural invariants
- Declaring hardware boundaries
- Specifying deterministic requirements
- Mode-aware generation constraints

### Section 2. Machine Code Validation Checklist
- Register state verification
- Control flow correctness checks
- Memory safety verification
- Privilege and mode compliance checks

### Section 3. Deterministic Testing Templates
- Reproducible test harness design
- Deterministic replay workflow
- Fault injection pattern
- Regression validation process
- Appendix E. Minimal Reference Skeletons

### Section 1. Minimal Boot Entry Skeleton
- Stack setup pattern
- Control transfer template
- Early initialization sequence

### Section 2. Minimal Long Mode Entry Skeleton
- GDT load pattern
- Paging activation template
- 64-bit entry jump structure

### Section 3. Minimal Interrupt and Scheduler Skeleton
- IDT installation pattern
- Basic interrupt handler template
- Cooperative task switch structure
---
