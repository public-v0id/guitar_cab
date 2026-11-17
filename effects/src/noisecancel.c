#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../effect.h"

#define NOISELENMAX 256

effect_t eff;

size_t noiseLen;

void effect(float *buffer, size_t bufsz) {
	for (size_t i = 0; i < bufsz; ++i) {
		buffer[i] *= 1000;
		if ((abs(buffer[i]) <= eff.param[0]) && (noiseLen < NOISELENMAX)) {
			noiseLen++;
			buffer[i] /= 1000;
			continue;
		}
		if ((abs(buffer[i]) <= eff.param[0]) && (noiseLen >= NOISELENMAX)) {
			buffer[i] = 0;
			continue;
		} 
		if ((abs(buffer[i]) > eff.param[0])) {
			noiseLen = 0;
			buffer[i] /= 1000;
			continue;
		}
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
	eff.param = malloc(sizeof(float)*1);
	eff.paramname = malloc(sizeof(char*)*1);
	eff.maxparamval = malloc(sizeof(float)*1);
	eff.maxparamval[0] = 1;
	eff.minparamval = malloc(sizeof(float)*1);
	eff.minparamval[0] = 0;
	eff.paramdelta = malloc(sizeof(float)*1);
	eff.paramdelta[0] = 0.01;
	for (int i = 0; i < 1; ++i) {
		eff.paramname[i] = malloc(sizeof(char)*16);
	}
	snprintf(eff.name, 32, "Noise Cancel");
	eff.param[0] = 0.01;
	snprintf(eff.paramname[0], 16, "NoiseLvl");
	eff.paramsize = 1;
	noiseLen = 255;
	eff.effect = effect;
	eff.destroy = destroy;
}
