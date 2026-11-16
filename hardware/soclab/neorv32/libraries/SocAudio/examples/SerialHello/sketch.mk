# SIM build settings (arduino/Makefile): tb_soc sends "ping" after the first line
SIM_TIME_SCALE ?= 1000
SIM_MS ?= 15
SIM_ARGS ?= expect=$(SKETCH_DIR)/sim_expect.txt uart_rx=ping uart_rx_after=1 stop_when_done=1
