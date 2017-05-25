include variables.mak

CC = gcc
CFLAGS = -Wall -O3 -ggdb
LDFLAGS = -lpthread -ljson-c -lm -lstdc++ -ldl -levent

SRC_DIR ?= .
export OBJ_DIR = obj_dir
export MOD_DIR = $(SRC_DIR)/modules

SRCS_SIM_ABSPATH = $(wildcard $(SRC_DIR)/*.c)
SRCS_SIM = $(notdir $(SRCS_SIM_ABSPATH))
SRCS_SIM_CPP = dut_init.cpp $(SRC_DIR)/lambdaveril.cpp
OBJS_SIM = $(SRCS_SIM:.c=.o)

all: modules sim

$(OBJS_SIM): %.o: $(SRC_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) -c $(CFLAGS) -o $(OBJ_DIR)/$@ $<

.PHONY: sim
sim: $(OBJS_SIM)

	verilator -Wno-fatal -O3 --cc dut.v --exe \
		$(SRCS_SIM_CPP) $(OBJS_SIM) \
		-CFLAGS "$(CFLAGS) -I$(SRC_DIR)" \
		-LDFLAGS "$(LDFLAGS)" \
		-trace

	make -j -C $(OBJ_DIR) -f Vdut.mk Vdut

.PHONY: modules
modules:
	make -f $(MOD_DIR)/Makefile

.PHONY: clean
clean:
	make -C $(MOD_DIR) clean
	rm -rf $(OBJ_DIR)
