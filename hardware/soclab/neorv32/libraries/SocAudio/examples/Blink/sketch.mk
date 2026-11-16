# SIM build settings (arduino/Makefile): 250 ms -> 250 us, course blinky checks
SIM_TIME_SCALE ?= 1000
SIM_MS ?= 8
SIM_ARGS ?= checks=blinky expect=$(SKETCH_DIR)/sim_expect.txt
