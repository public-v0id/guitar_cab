#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../effect.h"
/*
char *name;
float *param; //vol, fuzz
char **paramname;
int paramsize;
float *maxparamval;
float *minparamval;
float *paramdelta;
*/

effect_t eff;

void effect(float *buffer, size_t bufsz) {
	for (size_t i = 0; i < bufsz; ++i) {
		buffer[i] *= (1 << (int)(eff.param[1] * 10));
		if (buffer[i] > 0.5) buffer[i] = 0.5;
		if (buffer[i] < -0.5) buffer[i] = -0.5;
		buffer[i] *= eff.param[0];
	}
}

void destroy() {
	free(eff.name);
	free(eff.param);
	free(eff.maxparamval);
	free(eff.minparamval);
	free(eff.paramdelta);
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
	snprintf(eff.name, 32, "Fuzz");
	eff.param[0] = 1.0;
	eff.param[1] = 0.8;
	snprintf(eff.paramname[0], 16, "Vol");
	snprintf(eff.paramname[1], 16, "Fuzz");
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
	eff.effect = effect;
	eff.destroy = destroy;
}
