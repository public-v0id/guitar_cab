#ifndef EFFECT_H
#define EFFECT_H

typedef struct {
	void (*init)();
	void (*effect)(float*, size_t);
	void (*destroy)();
	char *name;
	char **paramname;
	float *param;
	float *maxparamval;
	float *minparamval;
	float *paramdelta;
	int paramsize;
	int on;
} effect_t;

#endif
