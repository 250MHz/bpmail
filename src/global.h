#ifndef GLOBAL_H
#define GLOBAL_H

/*
 * Using -std disables lowercase system-specific predefined macros which are
 * not allowed by the C standard. ION relies on platform.h to include pthread.h
 * and sys/time.h which uses said macros, but using -std does not include these
 * headers on Linux w/ glibc. This will be fixed eventually, so we need to it
 * ourselves for now.
 */
#include <pthread.h>
#include <sys/time.h>

#endif /* GLOBAL_H */

