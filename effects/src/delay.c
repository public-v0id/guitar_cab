#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../effect.h"

#define DELAYBUF_SZ 131072

effect_t eff;
float *delaybuf;

void effect(float *buffer, size_t bufsz) {
	static size_t curpos = 0;
	static size_t windowsize = 30000;
	size_t time = (size_t)(DELAYBUF_SZ*eff.param[0]);
	for (size_t i = 0; i < bufsz; ++i) {
		size_t oldpos = (i+curpos-windowsize+time) % time;
		buffer[i] += eff.param[1]*delaybuf[oldpos];
		if (buffer[i] > 1.0) buffer[i] = 1.0;
		else if (buffer[i] < -1.0) buffer[i] = -1.0;
	}
	memcpy(delaybuf+curpos, buffer, sizeof(float)*bufsz);
	curpos += bufsz;
	if (curpos >= time) {
		curpos -= time;
	}
}

void destroy() {
	free(eff.name);
	free(eff.param);
	free(eff.maxparamval);
	free(eff.minparamval);
	free(eff.paramdelta);
	free(delaybuf);
	for (int i = 0; i < eff.paramsize; ++i) {
		free(eff.paramname[i]);
	}
	free(eff.paramname);
}

void init() {
	eff.name = malloc(sizeof(char)*32);
	eff.param = malloc(sizeof(float)*2);
	eff.paramname = malloc(sizeof(char*)*2);
	for (int i = 0; i < 2; ++i) {
		eff.paramname[i] = malloc(sizeof(char)*16);
	}
	snprintf(eff.name, 32, "Delay");
	eff.param[0] = 0.8;
	eff.param[1] = 0.4;
	snprintf(eff.paramname[0], 16, "Time");
	snprintf(eff.paramname[1], 16, "Mix");
	eff.paramsize = 2;
	eff.maxparamval = malloc(sizeof(float)*eff.paramsize);
	eff.maxparamval[0] = 1;
	eff.maxparamval[1] = 1;
	eff.minparamval = malloc(sizeof(float)*eff.paramsize);
	eff.minparamval[0] = 0;
	eff.minparamval[1] = 0;
	eff.paramdelta = malloc(sizeof(float)*eff.paramsize);
	eff.paramdelta[0] = 0.01;
	eff.paramdelta[1] = 0.01;
	delaybuf = malloc(DELAYBUF_SZ*sizeof(float));
	eff.effect = effect;
	eff.destroy = destroy;
}
