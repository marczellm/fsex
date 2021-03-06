
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#include "common.h"
#include "midi.h"
#include "xv.h"
#include "library.h"
#include "sex.h"

#define NAME "fsex"
#define DEFAULT_ADDR "20:0"


static void usage()
{
	printf(
"Usage: " NAME " [options] [action] [sources [destination]]\n"
"\navailable options:\n"
"    -a --address port	ALSA sequencer port (default " DEFAULT_ADDR ")\n"
"    -d --device id	MIDI device ID (default 0x10)\n"
"    -f --force		Overwrite destination if already exists\n"
"    -q --quiet		Don't print messages to stdout\n"
"\navailable actions:\n"
"    -D --detect		Detect synth model\n"
"    -h --help		Show short description and exit\n"
"    -r --receive name	Retrieve bank b patch n from Juno-G\n"
"    -s --send num	Send temporary patch to Juno-G\n"
"    -V --version	Print version information\n"
	);
}

#define OPTIONS "a:Dd:hqrsV"
static struct option lopt[] = {
	{ "address",		1, 0, 'a' },
	{ "detect",		0, 0, 'D' },
	{ "device",		1, 0, 'd' },
	{ "help",		0, 0, 'h' },
	{ "quiet",		0, 0, 'q' },
	{ "receive",		0, 0, 'r' },
	{ "send",		0, 0, 's' },
	{ "version",            0, 0, 'V' },
};

int main(int argc, char **argv)
{
	int i, o, optidx;
	int action, force, num_in, num_out;
	char **file_in, *file_out, *addr;
	char *fname, *spec;
	struct fsex_libdata *lib_in;
	int dev_id;

	addr = DEFAULT_ADDR;
	action = force = 0;
	dev_id = 0x10;

	while ((o = getopt_long(argc, argv, OPTIONS, lopt, &optidx)) > 0) {
		switch (o) {
		case 'a':
			addr = optarg;
			break;
		case 'D':
			if (midi_open(NAME, addr) < 0) {
				fprintf(stderr,
					"error: can't open sequencer\n");
				exit(1);
			}
			show_id(sysex_get_id(dev_id));
			midi_close();
			exit(0);
		case 'd':
			dev_id = strtoul(optarg, NULL, 0);
			break;
		case 'h':
			usage();
			exit(0);
		case 'q':
			dup2(open("/dev/null", O_WRONLY), STDOUT_FILENO);
			break;
		case 'r':
		case 's':
			action = o;
			break;
		case 'V':
			printf(NAME " " VERSION "\n");
			exit(0);
		default:
			exit(1);
		}
	}

	if (!action) {
		fprintf(stderr, NAME ": no action specified\n");
		fprintf(stderr, "Run `" NAME " -h' for more information\n");
		exit(1);
	}

	/* detect and send have no output */
	num_in = argc - 1 - optind + (action == 's');
	num_out = (action == 'r');
	_D(_D_INFO "num_in = %d, num_out = %d", num_in, num_out);

	lib_in = malloc(num_in * sizeof(struct fsex_libdata));
	file_in = &argv[optind];
	if (num_out) file_out = argv[argc - 1];

	if (num_in < 1) {
		fprintf(stderr, NAME ": patch source required\n");
		exit(1);
	}

	if (num_out != 1 && action == 'e') {
		fprintf(stderr, NAME ": output library file name required\n");
		exit(1);
	}


	switch (action) {
	case 's':
		parse_spec(file_in[0], &fname, &spec);
		map_lib_file(fname, &lib_in[0]);
		set_list_flag(&lib_in[0], spec);

		if (midi_open(NAME, addr) < 0) {
			fprintf(stderr, "error: can't open sequencer\n");
			exit(1);
		}

		/* Send only one patch to temporary area */
		send_patch(&lib_in[0], dev_id);
		midi_close();
		exit(0);
	case 'r':
		for (i = 0; i < num_in; i++) {
			_D(_D_WARN "file_in[%d] = \"%s\"", i, file_in[i]);
			parse_spec(file_in[i], &fname, &spec);
			map_synth_patches(fname, &lib_in[i]);
			set_list_flag(&lib_in[i], spec);
		}

		if (midi_open(NAME, addr) < 0) {
			fprintf(stderr, "error: can't open sequencer\n");
			exit(1);
		}
		get_patches(dev_id, file_in, lib_in, num_in, file_out);
		midi_close();
		exit(0);
	}

	return 0;
}
