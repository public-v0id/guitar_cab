#ifndef EFFECTS_LIST_H
#define EFFECTS_LIST_H

#include "effect.h"

typedef struct effects_list {
	effect_t *effect;
	void *handle;
	struct effects_list *next;
} effects_list_t;

int pushEffect(effects_list_t **list_ptr, char *efpath);

void pullEffect(effects_list_t **list_ptr);

void freeEffects(effects_list_t **list_ptr);

#endif
