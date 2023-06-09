/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright (C) 2016 Intel Corporation.
 *   All rights reserved.
 */

/* Simple JSON "cat" utility */

#include "spdk/stdinc.h"

#include "spdk/json.h"
#include "spdk/file.h"

static void
usage(const char *prog)
{
	printf("usage: %s [-c] [-f] file.json\n", prog);
	printf("Options:\n");
	printf("-c\tallow comments in input (non-standard)\n");
	printf("-f\tformatted output (default: compact output)\n");
}

static void
print_json_error(FILE *pf, int rc, const char *filename)
{
	switch (rc) {
	case SPDK_JSON_PARSE_INVALID:
		fprintf(pf, "%s: invalid JSON\n", filename);
		break;
	case SPDK_JSON_PARSE_INCOMPLETE:
		fprintf(pf, "%s: incomplete JSON\n", filename);
		break;
	case SPDK_JSON_PARSE_MAX_DEPTH_EXCEEDED:
		fprintf(pf, "%s: maximum nesting depth exceeded\n", filename);
		break;
	default:
		fprintf(pf, "%s: unknown JSON parse error\n", filename);
		break;
	}
}

static int
json_write_cb(void *cb_ctx, const void *data, size_t size)
{
	FILE *f = cb_ctx;
	size_t rc;

	rc = fwrite(data, 1, size, f);
	return rc == size ? 0 : -1;
}

static int
process_file(const char *filename, FILE *f, uint32_t parse_flags, uint32_t write_flags)
{
	size_t size;
	void *buf, *end;
	ssize_t rc;
	struct spdk_json_val *values;
	size_t num_values;
	struct spdk_json_write_ctx *w;

	buf = spdk_posix_file_load(f, &size);
	if (buf == NULL) {
		fprintf(stderr, "%s: file read error\n", filename);
		return 1;
	}

	rc = spdk_json_parse(buf, size, NULL, 0, NULL, parse_flags);
	if (rc <= 0) {
		print_json_error(stderr, rc, filename);
		free(buf);
		return 1;
	}

	num_values = (size_t)rc;
	values = calloc(num_values, sizeof(*values));
	if (values == NULL) {
		perror("values calloc");
		free(buf);
		return 1;
	}

	rc = spdk_json_parse(buf, size, values, num_values, &end,
			     parse_flags | SPDK_JSON_PARSE_FLAG_DECODE_IN_PLACE);
	if (rc <= 0) {
		print_json_error(stderr, rc, filename);
		free(values);
		free(buf);
		return 1;
	}

	w = spdk_json_write_begin(json_write_cb, stdout, write_flags);
	if (w == NULL) {
		fprintf(stderr, "json_write_begin failed\n");
		free(values);
		free(buf);
		return 1;
	}

	spdk_json_write_val(w, values);
	spdk_json_write_end(w);
	printf("\n");

	if (end != buf + size) {
		fprintf(stderr, "%s: garbage at end of file\n", filename);
		free(values);
		free(buf);
		return 1;
	}

	free(values);
	free(buf);
	return 0;
}

int
main(int argc, char **argv)
{
	FILE *f;
	int ch;
	int rc;
	uint32_t parse_flags = 0, write_flags = 0;
	const char *filename;

	while ((ch = getopt(argc, argv, "cf")) != -1) {
		switch (ch) {
		case 'c':
			parse_flags |= SPDK_JSON_PARSE_FLAG_ALLOW_COMMENTS;
			break;
		case 'f':
			write_flags |= SPDK_JSON_WRITE_FLAG_FORMATTED;
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (optind == argc) {
		filename = "-";
	} else if (optind == argc - 1) {
		filename = argv[optind];
	} else {
		usage(argv[0]);
		return 1;
	}

	if (strcmp(filename, "-") == 0) {
		f = stdin;
	} else {
		f = fopen(filename, "r");
		if (f == NULL) {
			perror("fopen");
			return 1;
		}
	}

	rc = process_file(filename, f, parse_flags, write_flags);

	if (f != stdin) {
		fclose(f);
	}

	return rc;
}
