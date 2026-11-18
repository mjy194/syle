# Native PM-countdown baseline; include after firmware-halt.mk.
build/fhalt-observer.o: KFLAGS += -DFPM_DELAY_EXIT_CAP=131072 -DSYLE_FIRMWARE_HALT_PROBE
