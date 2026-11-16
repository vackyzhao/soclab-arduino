# SIM build settings (arduino/Makefile): 600 ms chords -> 60 ms, 100 ms gaps -> 10 ms
SIM_TIME_SCALE ?= 10
SIM_MS ?= 190
SIM_ARGS ?= expect=$(SKETCH_DIR)/sim_expect.txt stop_when_done=1 mono=1
