#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include <ncurses.h>
#include <pthread.h>

#include "effects_list.h"

#define BLOCK_FRAMES 256
#define INT16MAXVAL 32767.0

#define set_hw(funcparam) err = snd_pcm_hw_params_set_##funcparam(handle, params, funcparam);\
				if (err < 0) {\
					fprintf(stderr, "Error setting %s: %s\n", funcparam, snd_strerror(err));\
					return err;\
				}



//effect_t effects[16];

char effects[16][32];

int effects_cnt = 0;

snd_pcm_format_t format = SND_PCM_FORMAT_S16;
unsigned int rate = 44100;
unsigned int channels = 1;
unsigned int buffer_time = 500000;
unsigned int period_time = 100000;
double freq = 440;
int rate_resample = 1;

snd_pcm_sframes_t buffer_size;
snd_pcm_sframes_t period_size;
snd_output_t *output = NULL;

int16_t ibuffer[BLOCK_FRAMES];
int16_t obuffer[BLOCK_FRAMES];
float ebuffer[BLOCK_FRAMES];

int exit_val = 0;

effects_list_t *effects_list;

pthread_mutex_t chain_mtx;

void *graph(void *arg) {
	initscr();
	noecho();
	nodelay(stdscr, 1);
	keypad(stdscr, 1);
	int oldrow = 0;
	int oldcol = 0;
	int selrow = 0;
	int changes = 0;
	int mode = 0; // 0 for main mode, 1 for effect selection
	while (!exit_val) {
		int row;
		int col;
		int currow = 0;
		getmaxyx(stdscr, row, col);
		if ((oldrow != row) || (oldcol != col)) {
			changes = 1;
			oldrow = row;
			oldcol = col;
		}
		if (changes) {
			clear();
			switch (mode) {
				case 0:
					for (effects_list_t *cureff = effects_list; cureff != NULL; cureff = cureff->next) {
						move(currow++, 0);
						printw("%s %s", cureff->effect->name, (cureff->effect->on ? "ON" : "OFF"));
						for (int j = 0; j < cureff->effect->paramsize; ++j) {
							move(currow++, 4);
							printw("%s\t%f", cureff->effect->paramname[j], cureff->effect->param[j]);
						}
					}
					mvchgat(selrow, 0, col, A_REVERSE, 0, NULL);
					break;
				case 1:
					for (int i = 0; i < effects_cnt; ++i) {
						move(currow++, 0);
						printw("%s", effects[i]);
					}
					mvchgat(selrow, 0, col, A_REVERSE, 0, NULL);
					break;
			}
			refresh();
			changes = 0;
		}
		int ch = getch();
		if (ch != ERR) {
			switch (ch) {
				case 'q':
					if (mode == 0) {
						exit_val = 1;
						break;
					}
					mode = 0;
					changes = 1;
					break;
				case KEY_UP:
					if (selrow > 0) {
						mvchgat(selrow, 0, col, A_NORMAL, 0, NULL);
						selrow--;
						changes = 1;
					}
					break;
				case KEY_DOWN:
					mvchgat(selrow, 0, col, A_NORMAL, 0, NULL);
					selrow++;
					changes = 1;
					break;
				case KEY_LEFT: {
					if (mode != 0) break;
					int rval = 0;
					int found = 0;
					for (effects_list_t *cureff = effects_list; cureff != NULL; cureff = cureff->next) {
						if (rval == selrow) found = 1;
						rval++;
						for (int j = 0; j < cureff->effect->paramsize; ++j) {
							if (rval == selrow) {
								cureff->effect->param[j] -= cureff->effect->paramdelta[j];
								if (cureff->effect->param[j] < cureff->effect->minparamval[j]) {
									cureff->effect->param[j] = cureff->effect->minparamval[j];
								}
								changes = 1;
								found = 1;
							}
							rval++;
						}
					}
					}
					break;
				case KEY_RIGHT: {
					if (mode != 0) break;
					int rval = 0;
					int found = 0;
					for (effects_list_t *cureff = effects_list; cureff != NULL; cureff = cureff->next) {
						if (rval == selrow) found = 1;
						rval++;
						for (int j = 0; j < cureff->effect->paramsize; ++j) {
							if (rval == selrow) {
								cureff->effect->param[j] += cureff->effect->paramdelta[j];
								if (cureff->effect->param[j] > cureff->effect->maxparamval[j]) {
									cureff->effect->param[j] = cureff->effect->maxparamval[j];
								}
								changes = 1;
								found = 1;
							}
							rval++;
						}
					}
					}
					break;
				case ' ':
					if (mode == 1) {
						pthread_mutex_lock(&chain_mtx);
						pushEffect(&effects_list, effects[selrow]);
						pthread_mutex_unlock(&chain_mtx);
						mode = 0;
						changes = 1;
						break;
					}
					int rval = 0;
					int found = 0;
					for (effects_list_t *cureff = effects_list; cureff != NULL; cureff = cureff->next) {
						if (rval == selrow) {
							cureff->effect->on ^= 1;
							changes = 1;
							found = 1;
						}
						rval++;
						for (int j = 0; j < cureff->effect->paramsize; ++j) {
							if (rval == selrow) found = 1;
							rval++;
						}
					}
					break;
				case '+':
					if (mode == 1) break;
					mode = 1;
					changes = 1;
					break;
				case '-':
					pthread_mutex_lock(&chain_mtx);
					pullEffect(&effects_list);
					pthread_mutex_unlock(&chain_mtx);
					changes = 1;
					break;
			}
		}
	}
	endwin();
}

int list_devices(char *type, char *res) {
	int err = 0;
	int choice;
	void **hints;

	err = snd_device_name_hint(-1, "pcm", &hints);
	if (err < 0) {
		fprintf(stderr, "Failed to get device hints: %s\n", snd_strerror(err));
		return 1;
	}

	printf("Available ALSA %s devices:\n", type);
	for (void **n = hints; *n != NULL; ++n) {
		char *name = snd_device_name_get_hint(*n, "NAME");
		char *iodir = snd_device_name_get_hint(*n, "IOID");
		if (iodir == NULL || strcmp(iodir, type) == 0) {
			printf("%d - %s\n", n-hints, name ? name : "Unknown");
		}
		if (name) free(name);
		if (iodir) free(iodir);
	}

	scanf("%d", &choice);
	char *src = snd_device_name_get_hint(*(hints+choice), "NAME"); 
	strncpy(res, src, 31);
	if (src) free(src);

	snd_device_name_free_hint(hints);	
}

void i_to_e_buffer() {
	for (int i = 0; i < BLOCK_FRAMES; ++i) {
		ebuffer[i] = 1.0*ibuffer[i]/INT16MAXVAL;
	}
}

void e_to_o_buffer() {
	for (int i = 0; i < BLOCK_FRAMES; ++i) {
		obuffer[i] = (int16_t)(ebuffer[i]*INT16MAXVAL);
	}
}

int main(int argc, char **argv) {
	DIR *dir = opendir("./effects");
	if (dir == NULL) {
		fprintf(stderr, "Unable to open effects directory!");
	}
	struct dirent *entry;
	size_t cureff = 0;
	while ((entry = readdir(dir))) {
		printf("%s\n", entry->d_name);
		char efpath[32];
		snprintf(efpath, 32, "./effects/%s", entry->d_name);
		size_t len = strlen(efpath);
		if (len > 3 && efpath[len-1] == 'o' && efpath[len-2] == 's' && efpath[len-3] == '.') {
			memcpy(effects[cureff], efpath, 32);
			cureff++;
		}
/*		void *handle = dlopen(efpath, RTLD_LAZY);
		if (!handle) {
			continue;
		}
		printf("Found %s\n", efpath);
		dlerror();

		effects[cureff].init = (void*)dlsym(handle, "init");
		char *error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect init: %s\n", error);
			dlclose(handle);
			continue;
		}

		effects[cureff].effect = (void*)dlsym(handle, "effect");
		error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect: %s\n", error);
			dlclose(handle);
			continue;
		}

		effects[cureff].destroy = (void*)dlsym(handle, "destroy");
		error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect destructor: %s\n", error);
			dlclose(handle);
			continue;
		}

		effects[cureff].name = *((char**)dlsym(handle, "name"));
		error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect name: %s\n", error);
			dlclose(handle);
			continue;
		}

		effects[cureff].paramname = *((char***)dlsym(handle, "paramname"));
		error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect parameter names: %s\n", error);
			dlclose(handle);
			continue;
		}

		effects[cureff].param = *((float**)dlsym(handle, "param"));
		error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect parameters: %s\n", error);
			dlclose(handle);
			continue;
		}

		effects[cureff].maxparamval = *((float**)dlsym(handle, "maxparamval"));
		error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect parameters: %s\n", error);
			dlclose(handle);
			continue;
		}

		effects[cureff].minparamval = *((float**)dlsym(handle, "minparamval"));
		error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect parameters: %s\n", error);
			dlclose(handle);
			continue;
		}

		effects[cureff].paramdelta = *((float**)dlsym(handle, "paramdelta"));
		error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect parameters: %s\n", error);
			dlclose(handle);
			continue;
		}

		effects[cureff].paramsize = *((int*)dlsym(handle, "paramsize"));
		error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect parameters count: %s\n", error);
			dlclose(handle);
			continue;
		}
		
		effects[cureff].on = 0;
		*/
	}
	effects_cnt = cureff;

	char iname[32];
	char oname[32];

	if (list_devices("Input", iname)) {
		return 0;
	}

	if (list_devices("Output", oname)) {
		return 0;
	}

	int err;
	snd_pcm_t *ihandle;

	if ((err = snd_pcm_open(&ihandle, iname, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf(stderr, "Capture open error: %s\n", snd_strerror(err));
		return 0;
	}

	if ((err = snd_pcm_set_params(ihandle, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 1, 48000, 1, 5000)) < 0) {
		fprintf(stderr, "Capture open error: %s\n", snd_strerror(err));
		return 0;
	}

	snd_pcm_t *ohandle;

	if ((err = snd_pcm_open(&ohandle, oname, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		fprintf(stderr, "Playback open error: %s\n", snd_strerror(err));
		return 0;
	}

	if ((err = snd_pcm_set_params(ohandle, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 1, 48000, 1, 5000)) < 0) {
		fprintf(stderr, "Playback open error: %s\n", snd_strerror(err));
		return 0;
	}
	
	effects_list = NULL;

	pthread_mutex_init(&chain_mtx, NULL);

	pthread_t graph_thread;
	pthread_create(&graph_thread, NULL, graph, NULL);

	while (!exit_val) {
		snd_pcm_sframes_t ifs = snd_pcm_readi(ihandle, ibuffer, BLOCK_FRAMES);
		if (ifs == -EPIPE) snd_pcm_prepare(ihandle);
		else if (ifs < 0) fprintf(stderr, "Read error: %s\n", snd_strerror(err));
		i_to_e_buffer();
		pthread_mutex_lock(&chain_mtx);
		for (effects_list_t *effect = effects_list; effect != NULL; effect = effect->next) {
			if (effect->effect->on) effect->effect->effect(ebuffer, BLOCK_FRAMES);
		}
		pthread_mutex_unlock(&chain_mtx);
		e_to_o_buffer();
		snd_pcm_sframes_t ofs = snd_pcm_writei(ohandle, obuffer, ifs > 0 ? ifs : 0);
		if (ofs == -EPIPE) snd_pcm_prepare(ohandle);
		else if(ofs < 0) fprintf(stderr, "Write error: %s\n", snd_strerror(err));
	}
	
	freeEffects(&effects_list);
	pthread_mutex_destroy(&chain_mtx);
	snd_pcm_close(ohandle);
	snd_pcm_close(ihandle);
	return 0;
}
