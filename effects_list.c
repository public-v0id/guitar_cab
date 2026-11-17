#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <dirent.h>
#include "effects_list.h"

void *openSO(char *efpath, effect_t **effect) {
		void *handle = dlmopen(LM_ID_NEWLM, efpath, RTLD_NOW);
		if (!handle) {
			fprintf(stderr, "dlmopen: %s\n", dlerror());
			return NULL;
		}
		dlerror();

		*effect = (effect_t *)dlsym(handle, "eff");
		char *error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect name: %s\n", error);
			dlclose(handle);
			return NULL;
		}

		(*effect)->init = (void*)dlsym(handle, "init");
		error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect init: %s\n", error);
			dlclose(handle);
			return NULL;
		}

		(*effect)->on = 0;
		return handle;
}

int pushEffect(effects_list_t **list_ptr, char *efpath) {
	if (list_ptr == NULL || efpath == NULL) {
		return -1;
	}
	if (*list_ptr == NULL) {
		*list_ptr = malloc(sizeof(effects_list_t));
		effects_list_t *list = *list_ptr;
		if (list == NULL) {
			return -1;
		}
//		memcpy(&(list->effect), &effect, sizeof(effect_t));
		list->handle = openSO(efpath, &(list->effect));
		list->effect->init();
		list->next = NULL;
		return 0;
	}
	effects_list_t *list = *list_ptr;
	while (list->next != NULL) {
		list = (effects_list_t *)list->next;
	}
	list->next = malloc(sizeof(effects_list_t));
	if (list->next == NULL) {
		return -1;
	}
	list = (effects_list_t *)list->next;
//	memcpy(&(list->effect), &effect, sizeof(effect_t));
	list->handle = openSO(efpath, &(list->effect));
	list->effect->init();
	list->next = NULL;
	return 0;
}

void pullEffect(effects_list_t **list_ptr) {
	if (list_ptr == NULL) return;
	if (*list_ptr == NULL) return;
	effects_list_t *list = (effects_list_t *)(*list_ptr);
	if (list->next == NULL) {
		list->effect->destroy();
		dlclose(list->handle);
		free(list);
		*list_ptr = NULL;
		return;
	}
	while (list->next->next != NULL) {
		list = (effects_list_t *)list->next;
	}
	list->next->effect->destroy();
	dlclose(list->next->handle);
	free(list->next);
	list->next = NULL;
}

void freeEffects(effects_list_t **list_ptr) {
	if (list_ptr == NULL) return;
	if (*list_ptr == NULL) return;
	effects_list_t *list = (effects_list_t *)(*list_ptr);
	effects_list_t *next = (effects_list_t*)list->next;
	while (next != NULL) {
		list->effect->destroy();
		dlclose(list->handle);
		free(list);
		list = next;
		next = (effects_list_t*)list->next;
	}
	list->effect->destroy();
	dlclose(list->handle);
	free(list);
	*list_ptr = NULL;
}
