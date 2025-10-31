#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "htslib/hts.h"
#include "htslib/hts_log.h"

/* Minimal logging implementation ------------------------------------------------ */

int hts_verbose = HTS_LOG_WARNING;

void hts_set_log_level(enum htsLogLevel level) {
	hts_verbose = (int)level;
}

enum htsLogLevel hts_get_log_level(void) {
	return (enum htsLogLevel)hts_verbose;
}

void hts_log(enum htsLogLevel severity, const char *context, const char *format, ...) {
    if ((int)severity > hts_verbose) {
		return;
	}
	fprintf(stderr, "[htslib:%s] ", context ? context : "log");
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	if (format && format[strlen(format) - 1] != '\n') {
		fputc('\n', stderr);
	}
}

/* Stubs for index helpers used by BGZF. These operations are not
 * required for Helios' usage of BGZF streams, so they simply succeed. */

int hts_idx_push(hts_idx_t *idx, int tid, hts_pos_t beg, hts_pos_t end, uint64_t offset, int is_mapped) {
	(void)idx;
	(void)tid;
	(void)beg;
	(void)end;
	(void)offset;
	(void)is_mapped;
	return 0;
}

int hts_idx_check_range(hts_idx_t *idx, int tid, hts_pos_t beg, hts_pos_t end) {
	(void)idx;
	(void)tid;
	(void)beg;
	(void)end;
	return 0;
}
