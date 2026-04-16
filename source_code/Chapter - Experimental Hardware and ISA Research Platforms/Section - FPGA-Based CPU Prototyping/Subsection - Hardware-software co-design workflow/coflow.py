#!/usr/bin/env python3
# Orchestrate build, program, run, and capture for FPGA CPU prototypes.
import subprocess, sys, time, serial, tempfile, pathlib

# Configuration - adjust to environment
TOOLCHAIN = "/opt/Xilinx/Vivado/2020.2/bin/vivado"
PROJECT_TCL = "build_project.tcl"
BITFILE = "output/top.bit"
SERIAL_PORT = "/dev/ttyUSB0"
BAUD = 115200
TEST_BINARY = "build/firmware.bin"
TIMEOUT = 10.0

def run(cmd, **kw):
    print("+", " ".join(cmd))
    subprocess.run(cmd, check=True, **kw)

def build_bitstream():
    run([TOOLCHAIN, "-mode", "batch", "-source", PROJECT_TCL])

def program_fpga(bitfile):
    run(["vivado", "-mode", "batch", "-source", "program_bitfile.tcl", "-tclargs", bitfile])

def capture_serial(port, baud, timeout):
    with serial.Serial(port, baud, timeout=timeout) as s:
        out = []
        start = time.time()
        while time.time() - start < timeout:
            line = s.readline()
            if not line:
                continue
            decoded = line.decode('ascii', errors='replace').rstrip()
            out.append(decoded)
            # stop early on sentinel
            if "TEST_COMPLETE" in decoded:
                break
    return out

def flash_and_run():
    # Example: use openFPGALoader or vendor tool to write firmware to on-board flash.
    run(["openFPGALoader", "--write", TEST_BINARY])

if __name__ == "__main__":
    build_bitstream()                    # synthesize and implement
    program_fpga(BITFILE)               # program fabric
    flash_and_run()                     # flash firmware
    logs = capture_serial(SERIAL_PORT, BAUD, TIMEOUT)
    for l in logs:
        print(l)
    # Basic validation: check timestamp markers emitted by firmware.
    assert any("RDTSC_START" in l for l in logs), "Missing start timestamp"
    assert any("RDTSC_END" in l for l in logs), "Missing end timestamp"
    print("Run validated.")