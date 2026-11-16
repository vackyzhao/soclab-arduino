# SIM build settings (arduino/Makefile): 8 lines of 48 characters + newline, 80 us after
# every character, sent once "send lines" is out (the synth is running by then)
SIM_MS ?= 60
SIM_ARGS ?= expect=$(SKETCH_DIR)/sim_expect.txt uart_rx=0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKL \
            uart_rx_after=2 uart_rx_gap_us=80 uart_rx_repeat=8 mono=1 stop_when_done=1
