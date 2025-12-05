#!/bin/bash

# Initialize previous values by reading the files once before starting
prev_read_bits=$(< total_read_bits)
prev_write_bits=$(< total_write_bits)

echo "--- Read Bps ---|--- Write Bps --|-- Total Errs --|-- Cont. Errs --"

while sleep 1; do
    # Read the current total values from the files
    current_read_bits=$(< total_read_bits)
    current_write_bits=$(< total_write_bits)
    errs=$(< err_cnt)
    continuity_errs=$(< continuity_err_cnt)

    # Calculate the difference (rate since the last check)
    read_bps=$((current_read_bits - prev_read_bits))
    write_bps=$((current_write_bits - prev_write_bits))

    # Print the values in formatted columns
    printf "%-15d|%-15d|%-15d|%-15d\n" "$read_bps" "$write_bps" "$errs" "$continuity_errs"

    # Update the "previous" values for the next loop iteration
    prev_read_bits=$current_read_bits
    prev_write_bits=$current_write_bits
done

