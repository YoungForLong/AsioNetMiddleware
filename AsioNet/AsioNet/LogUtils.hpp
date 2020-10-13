#pragma once
#include <iostream>
#include <cstdarg>
#include "WLog/WLog.h"

#define LOG_CONSOLE 1


#if LOG_CONSOLE
    #if WIN32
        #include "Windows.h"


        #define LOG(...) \
            HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE); \
            SetConsoleTextAttribute(handle, FOREGROUND_INTENSITY | FOREGROUND_RED); \
            std::cout << "[" << __TIMESTAMP__ << "]    "; \
	        fprintf(stderr, __VA_ARGS__); \
	        std::cout << std::endl << "in function: " << __FUNCTION__ << ", file: " << __FILE__ << ", line: " << __LINE__ << ": "  << std::endl;

        #define LOG_NON_SENSITIVE(...) \
            HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE); \
            SetConsoleTextAttribute(handle, 0x07); \
            std::cout << "[" << __TIMESTAMP__ << "]    "; \
	        fprintf(stderr, __VA_ARGS__); \
            std::cout << std::endl;



    #else
        #define LOG(...) \
            std::cout << __TIMESTAMP__ << " "; \
            fprintf(stderr, __VA_ARGS__); \
            std::cout << "    " << ": function: " << __FUNCTION__ << ", file: " << __FILE__ << ", line: " << __LINE__ << ": "  << std::endl;

        #define LOG_NON_SENSITIVE(...) \
            std::cout << __TIMESTAMP__ << " "; \
	        fprintf(stderr, __VA_ARGS__); \
	        std::cout << "    " << ": function: " << __FUNCTION__ << ", file: " << __FILE__ << ", line: " << __LINE__ << ": "  << std::endl;
    #endif
#else
#define LOG(...) \
do \
{ \
    char __buf[MAX_STR_LEN]; \
    snprintf(__buf, MAX_STR_LEN, __VA_ARGS__); \
    char __buf_ex[MAX_STR_LEN]; \
    snprintf(__buf_ex, MAX_STR_LEN, "%s; in function: [%s], file: [%s], line: [%d]", __buf,__FUNCTION__, __FILE__, __LINE__); \
    WLogMgr->Trace(ELogType::ELT_Error, __buf_ex); \
}while(0)


#define LOG_NON_SENSITIVE(...) \
    WLogMgr->Trace(ELogType::ELT_Info, __VA_ARGS__)

#endif

#define LOG_PRIORITY(priority, format, args) LOG(format, args)