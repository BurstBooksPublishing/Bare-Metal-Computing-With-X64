#!/usr/bin/env bash
# run_qemu_gdb.sh -- assumes kernel.elf (64-bit ELF with set entry point).
set -euo pipefail

IMG=disk.img            # optional raw disk image if needed
KERNEL=kernel.elf
GDB_PORT=1234

# Create minimal raw image if absent (optional)
if [ ! -f "$IMG" ]; then
  dd if=/dev/zero of="$IMG" bs=1M count=64 >/dev/null 2>&1
fi

# QEMU invocation:
# -machine q35: modern platform; -m 256: small memory; -nographic: serial on stdio
# -s: shorthand for -gdb tcp::1234 ; -S: freeze CPU at startup until GDB connects
# -nodefaults -no-acpi -net none: minimize nondeterministic devices
# -icount shift=0: deterministic instruction-count mode (TCG only)
qemu-system-x86_64 \
  -machine q35,accel=tcg \
  -cpu Haswell \
  -m 256M \
  -nographic \
  -nodefaults \
  -no-acpi \
  -net none \
  -drive file="$IMG",if=ide,format=raw \
  -kernel "$KERNEL" \
  -s -S \
  -icount shift=0 \
  -monitor stdio \
  -name baremetal-debug &

QEMU_PID=$!

# Wait for QEMU to open the gdb port
sleep 0.5

# Start gdb and connect; set architecture explicitly and load symbols
# Replace 'gdb' with 'gdb-multiarch' if needed on your platform.
gdb -q --nx \
  -ex "set architecture i386:x86-64" \
  -ex "file $KERNEL" \
  -ex "target remote :${GDB_PORT}" \
  -ex "set pagination off" \
  -ex "info registers" \
  -ex "layout regs" \
  -ex "break *0x$(printf '%x' $(readelf -h $KERNEL | awk '/Entry point/ {print strtonum(\"0x\"$3)}' ) )" \
  -ex "continue"

# When gdb exits, ensure QEMU is terminated.
kill $QEMU_PID 2>/dev/null || true