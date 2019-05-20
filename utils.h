/* Copyright (C) Jungo 2002 - 2004 */

#ifndef _WD_UTILS_H_
#define _WD_UTILS_H_

#if !defined(WIN16)
    #include "windrvr.h"
#endif

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum 
{
    OS_NOT_SET = 0,
    OS_CAN_NOT_DETECT,
    OS_WIN_95,
    OS_WIN_98,
    OS_WIN_NT_4,
    OS_WIN_NT_5,
    OS_LINUX,
    OS_SOLARIS,
    OS_VXWORKS,
    OS_OS2,
} OS_TYPE;

OS_TYPE get_os_type(void);

#if !defined MAX_PATH
    #define MAX_PATH 4096
#endif

#if !defined(WIN16)

void replace_slashes(char *sPath, BOOL dos_to_unix);

typedef void (DLLCALLCONV *HANDLER_FUNC)(void *pData);

#if !(defined(WIN32) || defined(WIN16)) || defined(WINCE) || defined(_MT) 
DWORD DLLCALLCONV ThreadStart(HANDLE *phThread, HANDLER_FUNC pFunc, void *pData);
void DLLCALLCONV ThreadWait(HANDLE hThread);
// For backward compatibility - wrapper to ThreadWait:
void DLLCALLCONV ThreadStop(HANDLE hThread);
#endif

DWORD DLLCALLCONV OsEventCreate(HANDLE *phOsEvent);
void DLLCALLCONV OsEventClose(HANDLE hOsEvent);
DWORD DLLCALLCONV OsEventWait(HANDLE hOsEvent, DWORD dwSecTimeout);
DWORD DLLCALLCONV OsEventSignal(HANDLE hOsEvent);
DWORD DLLCALLCONV OsEventReset(HANDLE hOsEvent);
DWORD DLLCALLCONV OsMutexCreate(HANDLE *phOsMutex);
void DLLCALLCONV OsMutexClose(HANDLE hOsMutex);
DWORD DLLCALLCONV OsMutexLock(HANDLE hOsMutex);
DWORD DLLCALLCONV OsMutexUnlock(HANDLE hOsMutex);

void DLLCALLCONV PrintDbgMessage(DWORD dwLevel, DWORD dwSection, 
    const char *format, ...);

void DLLCALLCONV SleepWrapper(DWORD dwMicroSecs);

int DLLCALLCONV GetPageSize(void);

BOOL DLLCALLCONV UtilGetFileSize(const PCHAR sFileName, DWORD *dwFileSize, 
    PCHAR sErrString);

DWORD DLLCALLCONV UtilGetStringFromUser(PCHAR pcString, DWORD dwSizeStr, 
    const CHAR *pcInputText, const CHAR *pcDefaultString);

DWORD DLLCALLCONV UtilGetFileName(PCHAR pcFileName, DWORD dwFileNameSize, 
    const CHAR *pcDefaultFileName);

void DLLCALLCONV UtilClrScr(void);

#endif // !defined(WIN16)
#if defined(LINUX) && !(defined(stricmp))
    #define stricmp strcasecmp
#endif
#if defined(WINCE) && !(defined(stricmp))
    #define stricmp _stricmp
#endif
#if defined(LINUX)
    #define INFINITE 0xffffffff 
#endif
#ifdef __cplusplus
}
#endif

#endif
