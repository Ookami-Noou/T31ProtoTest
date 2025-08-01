#ifndef PROTO_UTIL_H
#define PROTO_UTIL_H

#define _CRT_SECURE_NO_WARNINGS 1

#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef null
#ifdef NULL
#define null NULL
#else
#define null ((void*) 0)
#endif
#endif

#ifdef cplusplus
extern "C" {
#endif

	// Convert a string to a long integer, handling signs and invalid characters
	// if ivalid characters are found, it prints an error message and returns 0
	extern int64_t string_to_long(const char* str, int len, bool* success);

	/**
	 * get timestamp of current.
	 *
	 * @param [in] buffer - output for string formatted by `%02d:%02d:%02d`
	 */
	extern void format_current_time(char* buffer);

#ifdef cplusplus
}
#endif
#endif // !PROTO_UTIL_H

