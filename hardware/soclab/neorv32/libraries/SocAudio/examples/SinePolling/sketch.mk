# SIM build settings (arduino/Makefile): also the checks of the course dac_sin
SIM_TIME_SCALE ?= 1000
SIM_MS ?= 20
SIM_ARGS ?= checks=dac_sin expect=$(SKETCH_DIR)/sim_expect.txt mono=1
