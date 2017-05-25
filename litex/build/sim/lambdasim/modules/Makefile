CC = gcc
CFLAGS = -Wall -O3 -ggdb -fPIC -Werror
LDFLAGS = -levent -shared -fPIC

MOD_DIR ?= .
OBJ_DIR ?= .

$(info $(MOD_DIR))

SRCS_ABSPATH = $(wildcard $(MOD_DIR)/*.c)
SRCS = $(notdir $(SRCS_ABSPATH))
OBJS = $(SRCS:.c=.o)
SOS = $(SRCS:.c=.so)

all: $(SOS)

$(OBJS): %.o: $(MOD_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) -c $(CFLAGS) -I$(MOD_DIR)/.. -o $(OBJ_DIR)/$@ $<

$(SOS): %.so: %.o
	$(CC) $(LDFLAGS) -Wl,-soname,$@ -o $(OBJ_DIR)/$@ $(OBJ_DIR)/$<

ethernet.so: ethernet.o tapcfg
	$(CC) $(LDFLAGS) -Wl,-soname,$@ -o $(OBJ_DIR)/$@ $(OBJ_DIR)/$< \
		$(OBJ_DIR)/taplog.o $(OBJ_DIR)/tapcfg.o

tapcfg:
	$(CC) $(CFLAGS) -c -o $(OBJ_DIR)/taplog.o $(MOD_DIR)/tapcfg/taplog.c
	$(CC) $(CFLAGS) -c -o $(OBJ_DIR)/tapcfg.o $(MOD_DIR)/tapcfg/tapcfg.c

spartan3.so: spartan3.o
	$(CC) $(LDFLAGS) -Wl,-soname,$@ -o $(OBJ_DIR)/$@ $(OBJ_DIR)/$< -lSDL2 -lSDL2_image

.PHONY: clean
clean:
	rm -f *.o *.so
