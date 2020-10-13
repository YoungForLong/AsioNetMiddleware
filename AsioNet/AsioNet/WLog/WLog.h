#pragma once

#include "parallel_core/SafeSingleton.h"
#include "LogDefine.h"

#include <list>
#include <thread>
#include <atomic>

#if WIN32
#include <windows.h>
#endif


static const int MAX_STR_LEN = 512;


class WLog : public parallel_core::SafeSingleton<WLog>
{
    struct SLogContent
    {
        unsigned char  bType;
        unsigned short wYear;
        unsigned short wMonth;
        unsigned short wDate;
        char           szContent[MAX_STR_LEN];
    };

    // copy from util
    static void VerifyDirectory(const char* szDir);

public:
    WLog();
    // not safe, so call in main thread once & only
    void    Init(const char* strLogPath, const char* strLogName, bool bUseWorkThread = false, int iContentCount = 1);
    // WLog(const char* strLogPath, const char* strLogName, bool bUseWorkThread = false, int iContentCount = 1);
    virtual ~WLog();

public:
    // 写文件操作
    void    Trace(unsigned char bType, const char* strInfo, ...);

public:
    // 追加写日志开关
    void    AppendLogMask(unsigned char bType);
    // 移除写日志开关
    void    RemoveLogMask(unsigned char bType);

private:
    // 判断对应类型的写日志开关是否打开
    bool    PeekLogMask(unsigned char bType);
    // 为写日志准备好文件
    bool    PrepareLogFile(SLogContent* ptrContent);
    // 创建日志路径
    void    CreateLogPath();

private:
    // 从空闲列表中取一个内容容器
    SLogContent* AllocFreeLogContent();
    // 把一个内容容器还回到空闲列表中去
    void         ReleaseFreeLogContent(SLogContent* ptrContent);
    // 立即把日志写到文件中
    void         WriteImmediately(SLogContent* ptrContent);

private:
    // 工作线程处理函数
    void    LogWriteHandler();
    // 停止工作线程
    void    StopWorkThead();

private:
    FILE* m_pFileStream[ELT_Max];                 // 写日志文件流
    unsigned int             m_nLogMask;                             // 写日志开关
    char                     m_strLogPath[MAX_STR_LEN];              // 日志的路径
    char                     m_strLogName[MAX_STR_LEN];              // 日志的名字
    char                     m_strCurLogName[ELT_Max][MAX_STR_LEN];  // 当前日志的完整名称
    CRITICAL_SECTION         m_cs;                                   // 线程同步的临界区变量

    bool                     m_bUseWorkThread;                       // 是否使用工作线程写文件
    SLogContent* m_astContent;                           // 内存块
    std::atomic_bool         m_bStopWorkThread;                      // 结束工作线程标志
    std::thread              m_WorkThread;                           // 工作线程对象
    std::list<SLogContent*>  m_listFreeContent;                      // 空闲的日志内容容器
    CRITICAL_SECTION         m_cs4FreeContent;
    std::list<SLogContent*>  m_listUsedContent;                      // 待处理的日志内容容器
    CRITICAL_SECTION         m_cs4UsedContent;
};


#define WLogMgr WLog::instance()