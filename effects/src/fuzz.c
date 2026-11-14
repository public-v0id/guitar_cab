#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

char *name;
float *param; //vol, fuzz
char **paramname;
int paramsize;

void init() {
	name = malloc(sizeof(char)*32);
	param = malloc(sizeof(float)*2);
	paramname = malloc(sizeof(char*)*2);
	for (int i = 0; i < 2; ++i) {
		paramname[i] = malloc(sizeof(char)*16);
	}
	snprintf(name, 32, "Fuzz");
	param[0] = 1.0;
	param[1] = 0.8;
	snprintf(paramname[0], 16, "Vol");
	snprintf(paramname[1], 16, "Fuzz");
	paramsize = 2;
}

void effect(float *buffer, size_t bufsz) {
	for (size_t i = 0; i < bufsz; ++i) {
		buffer[i] *= (1 << (int)(param[1] * 10));
		if (buffer[i] > 0.5) buffer[i] = 0.5;
		if (buffer[i] < -0.5) buffer[i] = -0.5;
		buffer[i] *= param[0];
	}
}
