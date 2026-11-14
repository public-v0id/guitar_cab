#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DELAYBUF_SZ 131072

char *name;
float *param; //vol, fuzz
char **paramname;
int paramsize;

float *delaybuf;

void init() {
	name = malloc(sizeof(char)*32);
	param = malloc(sizeof(float)*2);
	paramname = malloc(sizeof(char*)*2);
	for (int i = 0; i < 2; ++i) {
		paramname[i] = malloc(sizeof(char)*16);
	}
	snprintf(name, 32, "Delay");
	param[0] = 0.8;
	param[1] = 0.4;
	snprintf(paramname[0], 16, "Time");
	snprintf(paramname[1], 16, "Mix");
	paramsize = 2;
	delaybuf = malloc(DELAYBUF_SZ*sizeof(float));
}

void effect(float *buffer, size_t bufsz) {
	static size_t curpos = 0;
	static size_t windowsize = 30000;
	size_t time = (size_t)(DELAYBUF_SZ*param[0]);
	for (size_t i = 0; i < bufsz; ++i) {
		size_t oldpos = (i+curpos-windowsize+time) % time;
		buffer[i] += param[1]*delaybuf[oldpos];
		if (buffer[i] > 1.0) buffer[i] = 1.0;
		else if (buffer[i] < -1.0) buffer[i] = -1.0;
	}
	memcpy(delaybuf+curpos, buffer, sizeof(float)*bufsz);
	curpos += bufsz;
	if (curpos >= time) {
		curpos -= time;
	}
}
