# SIM build settings (arduino/Makefile): the FIFO runs empty between the rounds
SIM_TIME_SCALE ?= 1000
SIM_MS ?= 20
SIM_ARGS ?= expect=$(SKETCH_DIR)/sim_expect.txt allow_stops=1 stop_when_done=1 mono=1
