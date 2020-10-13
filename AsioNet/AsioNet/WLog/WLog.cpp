#include "WLog.h"

#if WIN32
#include <ImageHlp.h>
#pragma comment(lib,"imagehlp.lib")
#endif

static const char* LogFileExternName[ELT_Max] = { "Key","Error","Warning","Info" };

void WLog::VerifyDirectory(const char* szDir)
{
#if WIN32
    auto ret = MakeSureDirectoryPathExists(szDir);
#else
    unsigned int nIndex = 1;
    while (nIndex < strlen(szDir) + 1)
    {
        if (*(szDir + nIndex) == '/' || *(szDir + nIndex) == '\0')
        {
            char parent[1024] = { 0 };
            strncpy(parent, szDir, nIndex);
            if (0 != access(parent, 6))
            {
                mkdir(parent, 0755);
            }
        }
        nIndex++;
    }
#endif
}

WLog::WLog()
    : m_nLogMask(0)
{
}

void WLog::Init(const char* strLogPath, const char* strLogName, bool bUseWorkThread, int iContentCount)
{
    m_bUseWorkThread = bUseWorkThread;
    for (int iLoop = 0; iLoop < ELT_Max; ++iLoop)
    {
        m_pFileStream[iLoop]      = NULL;
        m_strCurLogName[iLoop][0] = 0;
    }
        
    std::strncpy(m_strLogPath, strLogPath, MAX_STR_LEN);
    CreateLogPath();
        
    std::strncpy(m_strLogName, strLogName, MAX_STR_LEN);
        
    InitializeCriticalSection(&m_cs);
        
    // 如果不用工作线程来写文件一个缓冲区就够了
    if (!m_bUseWorkThread)
    {
        iContentCount = 1;
    }
        
    // 防止参数传入错误
#if WIN32
    iContentCount = max(iContentCount, 1);
#else
    iContentCount = std::max(iContentCount, 1);
#endif

    m_astContent = new SLogContent[iContentCount];
    for (int iIndex = 0; iIndex < iContentCount; ++iIndex)
    {
        m_listFreeContent.push_back(&m_astContent[iIndex]);
    }
        
    if (m_bUseWorkThread)
    {
        InitializeCriticalSection(&m_cs4FreeContent);
        InitializeCriticalSection(&m_cs4UsedContent);
        m_bStopWorkThread = false;
        m_WorkThread      = std::thread(&WLog::LogWriteHandler, this);
    }
}

//// 构造函数
//WLog::WLog(const char* strLogPath, const char* strLogName, bool bUseWorkThread/* = false*/, int iContentCount/* = 1*/)
//    : m_nLogMask(0)
//    , m_bUseWorkThread(bUseWorkThread)
//{
//    for (int iLoop = 0; iLoop < ELT_Max; ++iLoop)
//    {
//        m_pFileStream[iLoop]      = NULL;
//        m_strCurLogName[iLoop][0] = 0;
//    }
//
//    std::strcpy(m_strLogPath, strLogPath, MAX_STR_LEN);
//    CreateLogPath();
//
//    std::strcpy(m_strLogName, strLogName, MAX_STR_LEN);
//
//    InitializeCriticalSection(&m_cs);
//
//    // 如果不用工作线程来写文件一个缓冲区就够了
//    if (!m_bUseWorkThread)
//    {
//        iContentCount = 1;
//    }
//
//    // 防止参数传入错误
//    iContentCount = std::max(iContentCount, 1);
//
//    m_astContent = new SLogContent[iContentCount];
//    for (int iIndex = 0; iIndex < iContentCount; ++iIndex)
//    {
//        m_listFreeContent.push_back(&m_astContent[iIndex]);
//    }
//
//    if (m_bUseWorkThread)
//    {
//        InitializeCriticalSection(&m_cs4FreeContent);
//        InitializeCriticalSection(&m_cs4UsedContent);
//        m_bStopWorkThread = false;
//        m_WorkThread      = std::thread(&WLog::LogWriteHandler, this);
//    }
//}

// 析构函数
WLog::~WLog()
{
    if (m_bUseWorkThread)
    {
        StopWorkThead();

        DeleteCriticalSection(&m_cs4FreeContent);
        DeleteCriticalSection(&m_cs4UsedContent);
    }

    // 释放内存池
    delete[] m_astContent;
    m_astContent = NULL;

    // 释放临界区
    DeleteCriticalSection(&m_cs);

    // 关闭文件流
    for (int iLoop = 0; iLoop < ELT_Max; ++iLoop)
    {
        if (m_pFileStream[iLoop])
        {
            fclose(m_pFileStream[iLoop]);
            m_pFileStream[iLoop] = NULL;
        }
    }
}

// 写文件操作
void WLog::Trace(unsigned char bType, const char* strInfo, ...)
{
    if (NULL == strInfo || bType >= ELT_Max)
    {
        return;
    }

    if (!PeekLogMask(bType))
    {
        return;
    }

    try
    {
        // 进入临界区
        EnterCriticalSection(&m_cs);

        SLogContent* ptrContent = AllocFreeLogContent();
        if (NULL != ptrContent)
        {
            time_t curTime;
            time(&curTime);
            struct tm* pTimeInfo = localtime(&curTime);

            // 填充内容
            ptrContent->bType        = bType;
            ptrContent->wYear        = pTimeInfo->tm_year + 1900;
            ptrContent->wMonth       = pTimeInfo->tm_mon + 1;
            ptrContent->wDate        = pTimeInfo->tm_mday;
            ptrContent->szContent[0] = 0;
            snprintf(ptrContent->szContent, MAX_STR_LEN, "%02d:%02d:%02d ", pTimeInfo->tm_hour, pTimeInfo->tm_min, pTimeInfo->tm_sec);

            va_list arg_ptr;
            va_start(arg_ptr, strInfo);
            int iLen = strlen(ptrContent->szContent);
            vsnprintf(ptrContent->szContent + iLen, MAX_STR_LEN - iLen, strInfo, arg_ptr);
            va_end(arg_ptr);

            if (m_bUseWorkThread)
            {
                // 如果在停止工作线程了就不能再接受新的日志了
                if (!m_bStopWorkThread)
                {
                    // 进入临界区
                    EnterCriticalSection(&m_cs4UsedContent);

                    m_listUsedContent.push_back(ptrContent);

                    // 离开临界区
                    LeaveCriticalSection(&m_cs4UsedContent);
                }
            }
            else
            {
                WriteImmediately(ptrContent);
            }
        }

        // 离开临界区
        LeaveCriticalSection(&m_cs);
    }
    // 若发生异常，则先离开临界区，防止死锁
    catch (...)
    {
        LeaveCriticalSection(&m_cs);
    }
}

// 追加写日志开关
void WLog::AppendLogMask(unsigned char bType)
{
    m_nLogMask |= ((unsigned int)1 << bType);
}

// 移除写日志开关
void WLog::RemoveLogMask(unsigned char bType)
{
    m_nLogMask &= (~((unsigned int)1 << bType));
}

// 判断对应类型的写日志开关是否打开
bool WLog::PeekLogMask(unsigned char bType)
{
    return ((m_nLogMask & ((unsigned int)1 << bType)) != 0);
}

// 为写日志准备好文件
bool WLog::PrepareLogFile(SLogContent* ptrContent)
{
    if (NULL == ptrContent || ptrContent->bType >= ELT_Max)
    {
        return false;
    }

    char strFileName[MAX_STR_LEN] = { 0 };
    snprintf(strFileName, MAX_STR_LEN, "%s_%04d%02d%02d.%s", m_strLogName, ptrContent->wYear, ptrContent->wMonth, ptrContent->wDate, LogFileExternName[ptrContent->bType]);

    // 如果文件名字不匹配(初始化状态 or 跨天)或者文件还没被打开
    if (0 != strcmp(m_strCurLogName[ptrContent->bType], strFileName) || NULL == m_pFileStream[ptrContent->bType])
    {
        std::strncpy(m_strCurLogName[ptrContent->bType], strFileName, MAX_STR_LEN);

        // 如果之前有打开文件则先关闭
        if (NULL != m_pFileStream[ptrContent->bType])
        {
            fclose(m_pFileStream[ptrContent->bType]);
        }

        strFileName[0] = 0;
        std::strncat(strFileName, m_strLogPath, MAX_STR_LEN);
        std::strncat(strFileName, m_strCurLogName[ptrContent->bType], MAX_STR_LEN);

        // 以追加的方式打开文件流
        m_pFileStream[ptrContent->bType] = fopen(strFileName, "a+");
    }

    if (NULL == m_pFileStream[ptrContent->bType])
    {
        return false;
    }

    return true;
}

// 创建日志文件的路径
void WLog::CreateLogPath()
{
#if WIN32
#else
    size_t iLen = strlen(m_strLogPath);
    if (iLen >= MAX_STR_LEN)
    {
        iLen = MAX_STR_LEN - 1;
        m_strLogPath[iLen] = 0;
    }

    if (m_strLogPath[iLen - 1] != '/')
    {
        if (iLen < MAX_STR_LEN - 1)
        {
            m_strLogPath[iLen] = '/';
            m_strLogPath[iLen + 1] = 0;
        }
        else
        {
            m_strLogPath[iLen - 1] = '/';
        }
    }
#endif
    VerifyDirectory(m_strLogPath);
}

WLog::SLogContent* WLog::AllocFreeLogContent()
{
    SLogContent* ptrContent = NULL;

    if (m_bUseWorkThread)
    {
        // 进入临界区
        EnterCriticalSection(&m_cs4FreeContent);

        if (!m_listFreeContent.empty())
        {
            ptrContent = m_listFreeContent.front();
            m_listFreeContent.pop_front();
        }

        // 离开临界区
        LeaveCriticalSection(&m_cs4FreeContent);
    }
    else
    {
        ptrContent = m_astContent;
    }

    return ptrContent;
}

void WLog::ReleaseFreeLogContent(WLog::SLogContent* ptrContent)
{
    if (m_bUseWorkThread)
    {
        if (NULL != ptrContent)
        {
            // 进入临界区
            EnterCriticalSection(&m_cs4FreeContent);

            m_listFreeContent.push_back(ptrContent);

            // 离开临界区
            LeaveCriticalSection(&m_cs4FreeContent);
        }
    }
    else
    {
        // No need to do anything
    }
}

void WLog::WriteImmediately(WLog::SLogContent* ptrContent)
{
    if (NULL == ptrContent)
    {
        return;
    }

    // 如果文件没有准备好
    if (!PrepareLogFile(ptrContent))
    {
        return;
    }

    // 写日志信息到文件流
    fprintf(m_pFileStream[ptrContent->bType], "%s\n", ptrContent->szContent);
    fflush(m_pFileStream[ptrContent->bType]);
}

// 独立线程写文件处理函数
void WLog::LogWriteHandler()
{
    while (!m_bStopWorkThread)
    {
        SLogContent* ptrContent = NULL;

        // 进入临界区
        EnterCriticalSection(&m_cs4UsedContent);

        if (!m_listUsedContent.empty())
        {
            ptrContent = m_listUsedContent.front();
            m_listUsedContent.pop_front();
        }

        // 离开临界区
        LeaveCriticalSection(&m_cs4UsedContent);

        if (NULL != ptrContent)
        {
            WriteImmediately(ptrContent);
            ReleaseFreeLogContent(ptrContent);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 如果还有剩下的日志要写完

    EnterCriticalSection(&m_cs4UsedContent);

    while (!m_listUsedContent.empty())
    {
        SLogContent* ptrContent = m_listUsedContent.front();
        m_listUsedContent.pop_front();

        if (NULL != ptrContent)
        {
            WriteImmediately(ptrContent);
            ReleaseFreeLogContent(ptrContent);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    LeaveCriticalSection(&m_cs4UsedContent);
}

void WLog::StopWorkThead()
{
    if (!m_bUseWorkThread)
    {
        return;
    }

    m_bStopWorkThread = true;
    m_WorkThread.join();
}



