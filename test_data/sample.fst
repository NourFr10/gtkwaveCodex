# Pseudo FST file for demonstration
scope module top
signal clk wire input 1
value clk 0 0
value clk 5 1
value clk 10 0
signal reset wire input 1
value reset 0 1
value reset 12 0
scope module cpu
signal data bus output 8
value data 0 0x00
value data 8 0x3C
value data 16 0x15
signal state wire internal 3
value state 0 0
value state 6 3
value state 12 5
endscope
endscope
