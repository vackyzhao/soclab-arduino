# SIM build settings (arduino/Makefile): 200 ms notes -> 20 ms, 50 ms gaps -> 5 ms, tone 10 ms
SIM_TIME_SCALE ?= 10
SIM_MS ?= 280
SIM_ARGS ?= expect=$(SKETCH_DIR)/sim_expect.txt stop_when_done=1 mono=1
