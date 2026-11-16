# Two voices (variant FullScale..._v2), SIM: 250 ms notes -> 25 ms, 50 ms gaps -> 5 ms
SYNTH_VOICES ?= 2
SIM_TIME_SCALE ?= 10
SIM_MS ?= 150
SIM_ARGS ?= expect=$(SKETCH_DIR)/sim_expect.txt mono=1 stop_when_done=1
