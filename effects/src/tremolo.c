#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../../effect.h"

#define TREMOLOBUF_SZ 131072

effect_t eff;

float *tremolobuf;

size_t prevtime;

float PI;

size_t curtime = 0;

void effect(float *buffer, size_t bufsz) {
	static size_t curpos = 0;
	static size_t windowsize = 30000;
	size_t time = (size_t)(TREMOLOBUF_SZ*eff.param[0]);
	for (size_t i = 0; i < time; ++i) {
		tremolobuf[i] = (sin((2*PI*i)/time))/2;
		tremolobuf[i] *= (1.0+10*eff.param[1]);
		if (tremolobuf[i] > 0.5) {
			tremolobuf[i] = 0.5;
		}
		else if (tremolobuf[i] < -0.5) {
			tremolobuf[i] = -0.5;
		}
		tremolobuf[i] *= eff.param[2];
		tremolobuf[i] += 0.5;
	}
	for (size_t i = 0; i < bufsz; ++i) {
		buffer[i] *= tremolobuf[curtime];
		curtime++;
		if (curtime >= time) {
			curtime -= time;
		}
	}
}

void destroy() {
	free(eff.name);
	free(eff.param);
	free(eff.maxparamval);
	free(eff.minparamval);
	free(eff.paramdelta);
	free(tremolobuf);
	for (int i = 0; i < eff.paramsize; ++i) {
		free(eff.paramname[i]);
	}
	free(eff.paramname);
}

void init() {
	eff.name = malloc(sizeof(char)*32);
	eff.param = malloc(sizeof(float)*3);
	eff.paramname = malloc(sizeof(char*)*3);
	for (int i = 0; i < 3; ++i) {
		eff.paramname[i] = malloc(sizeof(char)*16);
	}
	snprintf(eff.name, 32, "Tremolo");
	eff.param[0] = 0.1;
	eff.param[1] = 1;
	eff.param[2] = 1;
	snprintf(eff.paramname[0], 16, "Period");
	snprintf(eff.paramname[1], 16, "Sharpness");
	snprintf(eff.paramname[2], 16, "Depth");
	eff.paramsize = 3;
	eff.maxparamval = malloc(sizeof(float)*eff.paramsize);
	eff.maxparamval[0] = 1;
	eff.maxparamval[1] = 1;
	eff.maxparamval[2] = 1;
	eff.minparamval = malloc(sizeof(float)*eff.paramsize);
	eff.minparamval[0] = 0;
	eff.minparamval[1] = 0;
	eff.minparamval[2] = 0;
	eff.paramdelta = malloc(sizeof(float)*eff.paramsize);
	eff.paramdelta[0] = 0.01;
	eff.paramdelta[1] = 0.01;
	eff.paramdelta[0] = 0.01;
	tremolobuf = malloc(TREMOLOBUF_SZ*sizeof(float));
	PI = (float)acos(-1.0);
	eff.effect = effect;
	eff.destroy = destroy;
	printf("Init success\n");
}
