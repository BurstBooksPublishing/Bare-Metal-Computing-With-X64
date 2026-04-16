# create primary clock on top-level port clk (5ns => 200MHz)
create_clock -name clk_cpu -period 5.0 [get_ports clk] 

# generated clocks from PLL/MMCM to fabric outputs (example)
create_generated_clock -name clk_mem -source [get_pins pll_inst/CLKOUT1] -divide_by 2 [get_pins mem_clk_buf/O]

# declare asynchronous clock groups (no timing relationship)
set_clock_groups -asynchronous -group {clk_cpu} -group {clk_mem}

# multi-cycle path for external DDR transactions handled by controller
set_multicycle_path -from [get_pins cpu_top/axi_master_inst/*] -to [get_pins ddr_ctrl/*] 2

# false path for debug scan chain and reset asynchronous signals
set_false_path -from [get_pins scan_chain/*] -to [get_registers cpu_regfile/*]

# constrain input/output delays relative to external memory interface
set_input_delay -clock clk_mem 2.0 [get_ports ddr_dq*]   ;# example input timing
set_output_delay -clock clk_mem 2.0 [get_ports ddr_dq*]  ;# example output timing

# set_max_delay for explicit long-path budget if microarch allows
set_max_delay -from [get_pins cpu_top/long_bypass_mux/*] -to [get_pins cpu_top/alu/*] 3.5