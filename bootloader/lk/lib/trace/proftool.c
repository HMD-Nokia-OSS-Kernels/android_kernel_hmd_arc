/*
# Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
# Licensed under the Unisoc General Software License, version 1.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
# Software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OF ANY KIND, either express or implied.
# See the Unisoc General Software License, version 1.0 for more details.
*/

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef unsigned int u32;
typedef unsigned long long u64;

#define MAX_LINE_LEN 500
#define SPRD_TRACE_MAGIC	0x44525053 /* "SPRD" */
int verbose = 3;	//control log level

enum ftrace_flags {
	FUNCF_EXIT		= 0UL << 30,
	FUNCF_ENTRY		= 1UL << 30,
	FUNCF_TEXTBASE		= 2UL << 30,

	FUNCF_TIMESTAMP_MASK	= 0x3fffffff,
};

struct trace_call {
	unsigned int func;		/* Function offset */
	unsigned int caller;	/* Caller function offset */
	unsigned int flags;		/* Flags and timestamp */
};

struct trace_hdr {
	u64 magic;	/*magic number*/
	u64 func_count;		/* Total number of function call sites */
	u64 call_count;		/* Total number of tracked function calls */
	u64 untracked_count;	/* Total number of untracked function calls */
	u64 ftrace_offset;	/*The offset of function call records*/
	u64 ftrace_num;	/* The number for trace_call struct we have space for */
	u64 ftrace_count;	/* The number of ftrace records written */
	u64 trace_size;
	u32 depth;
	u32 depth_limit;
};
struct trace_hdr *hdr;
char *call_start; //point the trace call start address
char **symlist; //point the list of symbol
int first_saved = 1;

static void outf(int level, const char *fmt, ...)
		__attribute__ ((format (__printf__, 2, 3)));
#define error(fmt, b...) outf(0, fmt, ##b)
#define warn(fmt, b...) outf(1, fmt, ##b)
#define notice(fmt, b...) outf(2, fmt, ##b)
#define info(fmt, b...) outf(3, fmt, ##b)
#define debug(fmt, b...) outf(4, fmt, ##b)


static void outf(int level, const char *fmt, ...)
{
	if (verbose >= level) {
		va_list args;

		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
	}
}

static void usage(void)
{
	fprintf(stderr,
		"Usage: proftool -m <symbol file> -b <trace file> -t <output txt file>\n"
		"\n"
		"default parameter is System.map, trace.bin, trace.txt"
		"\n");
	exit(EXIT_FAILURE);
}

static int find_caller_by_index(unsigned int raw_index)
{
	unsigned int low_index = 0; //least function that could be a match
	unsigned int caller_index;

	for(caller_index = raw_index; caller_index > low_index; caller_index--)
	{
		if(strlen(symlist[caller_index]) > 0)
			return caller_index;
	}
}

static int read_system_map(FILE *fin)
{
	unsigned long offset, symsize, start = 0;
	char buff[MAX_LINE_LEN];
	char symrng; //symbol range: local or global
	char symtype[3], symsec[12], symname[30];
	int linenum, index;
	int func_count = 0;

	symlist = (char **)malloc(sizeof(char*) * (hdr->func_count));
	for(int i = 0; i < hdr->func_count; i++) {
		symlist[i] = (char *)malloc(sizeof(char)*30);
	}
	assert(symlist);

	for (linenum = 1; linenum < hdr->func_count; linenum++)
	{
		int fields = 0;

		if (fgets(buff, sizeof(buff), fin)) {
			fields = sscanf(buff, "%lx %c %3s %12s %lx %100s\n", &offset, &symrng,
				symtype, symsec, &symsize, symname);
			// debug("len:%ld, buff is %s\n", strlen(buff), buff);
		}
		if (buff[0] == '\n'){ //skip the blank line
			continue;
		}

		if (fields != 6) {
			continue;
		} else if (feof(fin)) {
			break;
		}

		/* Must be a text symbol */
		if (strcmp(symtype, "F") && strcmp(symsec, ".text"))
			continue;
		debug("linenum %d: %s", linenum, buff);
		if (!func_count)
			start = offset;

		func_count++;
		index = (offset - start) / 4;
		strcpy(symlist[index], symname);
		debug("symlist[%d] is %s\n", index, symlist[index]);
	}
	notice("%d functions found in map file\n", func_count);

	return 0;
}

static int read_data(FILE *fin, void *buff, int size)
{
	int err;

	err = fread(buff, 1, size, fin);
	if (!err)
		return 1;
	if (err != size) {
		error("Cannot read file at pos %ld\n", ftell(fin));
		return -1;
	}
	return 0;
}

static int read_map_file(const char *fname)
{
	FILE *fmap;
	int err = 0;

	fmap = fopen(fname, "r");
	if (!fmap) {
		error("Cannot open map file '%s'\n", fname);
		return 1;
	}
	if (fmap) {
		err = read_system_map(fmap);
		fclose(fmap);
	}
	return err;
}

/*
 * # tracer: function
 * #
 * #           TASK-PID   CPU#    TIMESTAMP  FUNCTION
 * #              | |      |          |         |
 * #           bash-4251  [01] 10152.583854: path_put <-path_walk
 * #           bash-4251  [01] 10152.583855: dput <-path_put
 * #           bash-4251  [01] 10152.583855: _atomic_dec_and_lock <-dput
 */
static void save_title_2_txt(FILE *fp)
{
	char title[] = "# tracer: ftrace\n";
	char delim[] = "#\n";
	char note[] = "# In: func <- caller      Out: func -> caller\n";
	char category[] = "#           TASK-PID   CPU#  TIMESTAMP  FUNCTION\n";
	char delim1[] = "#             |  |      |      |         |\n";

	fwrite(title, 1, strlen(title), fp);
	fwrite(delim, 1, strlen(delim), fp);
	fwrite(note, 1, strlen(note), fp);
	fwrite(category, 1, strlen(category), fp);
	fwrite(delim1, 1, strlen(delim1), fp);
}

static void save_trace_2_txt(FILE *fp, unsigned int type, unsigned int timestamp, char *func, char *caller)
{
	char line[200];

	sprintf(line, "%16s-%-5d [1] %06d: %s %3s %s\n", "bootloader", 1, timestamp, func, (type?"<-":"->"), caller);
	info("%s", line);
	fwrite(line, 1, strlen(line), fp);
}

static int parse_trace(FILE *fin, FILE *fp)
{
	char *buff;
	unsigned int func_index, call_index;
	int call_size;
	struct trace_call *call_start, *curr_call;
	int ret = 0;
	unsigned int timestamp, type;
	char func[30], caller[30];

	call_size = hdr->trace_size - sizeof(struct trace_hdr);
	call_start = (struct trace_call *)malloc(call_size);
	debug("call size is %d, call start is %p, hdr->ftrace_count is %lld\n", call_size, call_start, hdr->ftrace_count);
	fseek(fin, sizeof(struct trace_hdr), SEEK_SET);
	ret = fread(call_start, 1, call_size, fin);
	if (ret != call_size)
		ret = -1;

	save_title_2_txt(fp);
	for (int i = 1; i < hdr->ftrace_count; i++) { //index 0 record text base address
		curr_call = call_start + i;
		timestamp = curr_call->flags & FUNCF_TIMESTAMP_MASK;
		type = curr_call->flags >> 30; //In or Out
		func_index = curr_call->func;
		call_index = find_caller_by_index(curr_call->caller);//the caller in struct trace_call is the address of the next line that calling the func
	 	debug("timestamp is %d, func: %s, caller:%s\n", timestamp, symlist[func_index], symlist[call_index]);
	 	save_trace_2_txt(fp, type, timestamp, symlist[func_index], symlist[call_index]);
	}

	return 0;
}

static int read_trace_file(const char *trace_fname, const char *txt_fname)
{
	FILE *fin, *fp;
	int err;

	fin = fopen(trace_fname, "r");
	fp = fopen(txt_fname, "w+");
	if (!fin) {
		error("Cannot open trace file '%s'\n", trace_fname);
		return -1;
	}
	debug("enter the parse_trace\n");
	err = parse_trace(fin, fp);
	fclose(fin);
	fclose(fp);

	return err;
}

static int read_trace_hdr(const char *fname, char *hdr_buf)
{
	FILE *fin;
	int ret;

	fin = fopen(fname, "r");
	if (!fin) {
		error("Cannot open trace file '%s'\n", fname);
		return -1;
	}
	ret = read_data(fin, hdr_buf, sizeof(struct trace_hdr));
	fclose(fin);
	return ret;
}

static int prof_tool(int argc, char * const argv[], const char *map_fname,
		     const char *trace_fname, const char *txt_fname)
{
	int err = 0;

	hdr = (struct trace_hdr *)malloc(sizeof(struct trace_hdr));
	//first, read trace header to verify magic
	if (!read_trace_hdr(trace_fname, (char*)hdr))
		debug("read header into the buf successfully\n");
	else {
		error("read head error\n");
		return -1;
	}

	if(hdr->magic == (unsigned int)SPRD_TRACE_MAGIC)
		debug("the trace header is ok!!!!\n");
	else {
		error("read header magic error, the hdr magic is 0x%llx\n", hdr->magic);
		return -1;
	}

	//second, save the symbol into **symlist
	if (read_map_file(map_fname))
		return -1;

	//third, parse the trace data and save the string into txt file
	if (read_trace_file(trace_fname, txt_fname))
		return -1;

	return 0;
}

int main(int argc, char *argv[])
{
	const char *map_fname = "System.map";
	const char *trace_fname = "trace.bin";
	const char *txt_fname = "trace.txt";
	int opt;

	if (argc < 2)
		usage();

	while ((opt = getopt(argc, argv, "m:b:t:")) != -1) {
		switch (opt) {
		case 'm':
			map_fname = optarg;
			debug("the map name is %s\n", map_fname);
			break;

		case 'b':
			trace_fname = optarg;
			break;

		case 't':
			txt_fname = optarg;
			break;

		default:
			usage();
		}
	}

	argc -= optind; argv += optind;

	debug("Debug enabled\n");
	return prof_tool(argc, argv, map_fname, trace_fname, txt_fname);
}
