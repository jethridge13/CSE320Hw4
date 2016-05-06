#include "sfwrite.h"
#include <stdarg.h>

void sfwrite(pthread_mutex_t *lock, FILE* stream, char *fmt, ...){
	va_list args;
	va_start(args, fmt);
	pthread_mutex_lock(lock);
	vfprintf(stream, fmt, args);
	pthread_mutex_unlock(lock);
}