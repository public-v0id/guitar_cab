CC=gcc
CAB_CFLAGS=-D_GNU_SOURCE -g
CAB_LDFLAGS=-ldl -lasound -lncurses
EFF_CFLAGS=-shared -fPIC
EFF_LDFLAGS=-lm

TARGET=guitar_cab
EFFECT_SRC:=$(wildcard effects/src/*.c)
EFFECTS:=$(patsubst effects/src/%.c,effects/%.so,$(EFFECT_SRC))

.PHONY: all clean

all: $(TARGET) $(EFFECTS)

$(TARGET): main.c effects_list.c
	$(CC) $(CAB_CFLAGS) $(CAB_LDFLAGS) effects_list.c main.c -o $@

effects/%.so: effects/src/%.c
	$(CC) $(EFF_CFLAGS) $(EFF_LDFLAGS) $< -o $@

clean:
	rm -f $(TARGET) $(EFFECTS)
