#ifndef _LOG_H_
#define _LOG_H_

#include <cstdlib>	

#ifdef __cplusplus
extern "C" {
#endif

static char* LOG_LABELS[]{ "I","D","E" };
enum log_level {
	LOG_INFO = 0,
	LOG_DEBUG = 1,
	LOG_ERROR = 2,
};

static void log_message(log_level level, const char *tag, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	fprintf(stdout, "[%s] %s: ", LOG_LABELS[level], tag);
	vfprintf(stdout, fmt, args);
	fprintf(stdout, "\n");

	va_end(args);
}

static void log_hex(const unsigned char c)
{
	fprintf(stdout, "0x%02X ", c);
}

#define LOG_I(...) log_message(LOG_INFO, TAG, __VA_ARGS__);
#define LOG_D(...) log_message(LOG_DEBUG, TAG, __VA_ARGS__);
#define LOG_E(...) log_message(LOG_ERROR, TAG, __VA_ARGS__);
#define LOG_HEX(...) log_hex(__VA_ARGS__);

#ifdef __cplusplus
}
#endif

#endif //_LOG_H_