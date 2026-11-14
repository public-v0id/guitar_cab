#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include <ncurses.h>
#include <pthread.h>

#define BLOCK_FRAMES 256
#define INT16MAXVAL 32767.0

#define set_hw(funcparam) err = snd_pcm_hw_params_set_##funcparam(handle, params, funcparam);\
				if (err < 0) {\
					fprintf(stderr, "Error setting %s: %s\n", funcparam, snd_strerror(err));\
					return err;\
				}

typedef struct {
	void (*effect)(float*, size_t);
	char *name;
	char **paramname;
	float *param;
	int paramsize;
	int on;
} effect_t;

effect_t effects[2];

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

void *graph(void *arg) {
	initscr();
	noecho();
	nodelay(stdscr, 1);
	keypad(stdscr, 1);
	int oldrow = 0;
	int oldcol = 0;
	int selrow = 0;
	int changes = 0;
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
			for (int i = 0; i < effects_cnt; ++i) {
				move(currow++, 0);
				printw("%s %s", effects[i].name, (effects[i].on ? "ON" : "OFF"));
				for (int j = 0; j < effects[i].paramsize; ++j) {
					move(currow++, 4);
					printw("%s\t%f", effects[i].paramname[j], effects[i].param[j]);
				}
			}
			mvchgat(selrow, 0, col, A_REVERSE, 0, NULL);
			refresh();
			changes = 0;
		}
		int ch = getch();
		if (ch != ERR) {
			switch (ch) {
				case 'q':
					exit_val = 1;
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
					int rval = 0;
					int found = 0;
					for (int i = 0; (i < effects_cnt) && !found; ++i) {
						if (rval == selrow) found = 1;
						rval++;
						for (int j = 0; (j < effects[i].paramsize) && !found; ++j) {
							if (rval == selrow) {
								effects[i].param[j] -= 0.01;
								changes = 1;
								found = 1;
							}
							rval++;
						}
					}
					}
					break;
				case KEY_RIGHT: {
					int rval = 0;
					int found = 0;
					for (int i = 0; !found && (i < effects_cnt); ++i) {
						if (rval == selrow) found = 1;
						rval++;
						for (int j = 0; !found && (j < effects[i].paramsize); ++j) {
							if (rval == selrow) {
							       effects[i].param[j] += 0.01;
							       changes = 1;
							       found = 1;
							}
							rval++;
						}
					}
					}
					break;
				case ' ':
					int rval = 0;
					int found = 0;
					for (int i = 0; !found && (i < effects_cnt); ++i) {
						if (rval == selrow) {
							effects[i].on ^= 1;
							changes = 1;
							found = 1;
						}
						rval++;
						for (int j = 0; !found && (j < effects[i].paramsize); ++j) {
							if (rval == selrow) found = 1;
							rval++;
						}
					}
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
		void *handle = dlopen(efpath, RTLD_LAZY);
		if (!handle) {
			continue;
		}
		printf("Found %s\n", efpath);
		dlerror();

		void (*init)(void) = (void*)dlsym(handle, "init");
		char *error = dlerror();
		if (!error) {
			init();
		}

		effects[cureff].effect = (void*)dlsym(handle, "effect");
		error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect: %s\n", error);
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

		effects[cureff].paramsize = *((int*)dlsym(handle, "paramsize"));
		error = dlerror();
		if (error) {
			fprintf(stderr, "Error getting effect parameters count: %s\n", error);
			dlclose(handle);
			continue;
		}
		
		effects[cureff].on = 0;
		cureff++;
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

	pthread_t graph_thread;
	pthread_create(&graph_thread, NULL, graph, NULL);

	while (!exit_val) {
		snd_pcm_sframes_t ifs = snd_pcm_readi(ihandle, ibuffer, BLOCK_FRAMES);
		if (ifs == -EPIPE) snd_pcm_prepare(ihandle);
		else if (ifs < 0) fprintf(stderr, "Read error: %s\n", snd_strerror(err));
		i_to_e_buffer();
		for (int i = 0; i < effects_cnt; ++i) {
			if (effects[i].on) effects[i].effect(ebuffer, BLOCK_FRAMES);
		}
		e_to_o_buffer();
		snd_pcm_sframes_t ofs = snd_pcm_writei(ohandle, obuffer, ifs > 0 ? ifs : 0);
		if (ofs == -EPIPE) snd_pcm_prepare(ohandle);
		else if(ofs < 0) fprintf(stderr, "Write error: %s\n", snd_strerror(err));
	}

	snd_pcm_close(ohandle);
	snd_pcm_close(ihandle);
	return 0;
}
