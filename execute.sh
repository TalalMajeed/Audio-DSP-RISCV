#!/bin/bash
set -e

echo "=========================================="
echo " Building firmware (firmware.hex) "
echo "=========================================="

make firmware/firmware.hex TOOLCHAIN_PREFIX=riscv64-unknown-elf-

echo ""
echo "=========================================="
echo " Building Verilator Testbench "
echo "=========================================="
echo ""

if [ ! -f "./testbench_verilator" ]; then
    echo "testbench_verilator not found — rebuilding..."
    make testbench_verilator
else
    echo "testbench_verilator exists — skipping build"
fi

echo ""
echo "=========================================="
echo " Running Verilator Testbench "
echo "=========================================="
echo ""

./testbench_verilator +noerror