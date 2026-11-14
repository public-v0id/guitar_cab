CC=gcc
CAB_CFLAGS=
CAB_LDFLAGS=-ldl -lasound -lncurses
EFF_CFLAGS=-shared -fPIC

TARGET=guitar_cab
EFFECT_SRC:=$(wildcard effects/src/*.c)
EFFECTS:=$(patsubst effects/src/%.c,effects/%.so,$(EFFECT_SRC))

.PHONY: all clean

all: $(TARGET) $(EFFECTS)

$(TARGET): main.c
	$(CC) $(CAB_CFLAGS) $(CAB_LDFLAGS) $< -o $@

effects/%.so: effects/src/%.c
	$(CC) $(EFF_CFLAGS) $< -o $@

clean:
	rm -f $(TARGET) $(EFFECTS)
