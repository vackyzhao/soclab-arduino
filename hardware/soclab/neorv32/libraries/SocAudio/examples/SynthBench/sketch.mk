# SIM build settings (arduino/Makefile): 20 ms settle -> 2 ms, 50 ms measurement -> 5 ms
SIM_TIME_SCALE ?= 10
SIM_MS ?= 150
SIM_ARGS ?= expect=$(SKETCH_DIR)/sim_expect.txt stop_when_done=1 mono=1
