#pragma once

#include <stddef.h>

// predictor
#ifdef __GNUC__
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

#ifdef _MSC_VER
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

// cpu processor num
size_t get_standard_thread_num();
#ifdef __linux__
#include "unistd.h"
#include "sys/sysinfo.h"

#else
#include "winsock2.h"
#include "windows.h"

#endif

#ifdef __linux__
#define GET_CURRENT_THREAD_ID getpid()
#else
#define GET_CURRENT_THREAD_ID GetCurrentThreadId()
#endif

#define SAFE_DELETE(obj) if(NULL != obj) {delete(obj);} obj = NULL

#define HAS_KEY(container, key) (container.find(key) != container.end())

#include <omp.h>