/* Threads.c -- multithreading library
2009-09-20 : Igor Pavlov : Public domain */

#ifndef _WIN32_WCE
#include <process.h>
#endif

#include "Threads.h"

#ifdef __OS2__
#include <stdlib.h>
#include <stdio.h>
#ifdef   ECS
#include <progress.h>
#endif

void OS2Abort(const char* message)
{
  PPIB ppib;
  DosGetInfoBlocks( NULL, &ppib );

  if (ppib->pib_ultype == 3) {
    #ifdef ECS
    ExMessageBox ( HWND_DESKTOP, HWND_DESKTOP, message, "Critical Error", 100,
                   MB_OK | MB_ERROR | MB_APPLMODAL | MB_PREFORMAT | MB_ANIMATED );
    #else
    WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, message, "Critical Error", 100,
                   MB_OK | MB_ERROR | MB_APPLMODAL | MB_MOVEABLE );
    #endif
  } else {
    fprintf(stderr, "%s\n", message);
    fflush(stderr);
  }
  abort();
}

WRes Thread_Create(CThread *p, THREAD_FUNC_TYPE func, PVOID param) {
  return DosCreateThread(p, (PFNTHREAD)func, (ULONG)param, CREATE_READY, 1048576);
}

WRes Event_Set(CEvent *p)
{
  WRes rc = DosPostEventSem(*p);

  switch (rc) {
    case ERROR_ALREADY_POSTED:
    case ERROR_TOO_MANY_POSTS:
      rc = NO_ERROR;
  }
  return rc;
}

WRes Event_Reset(CEvent *p)
{
  ULONG ulPostCt;
  WRes  rc = DosResetEventSem(*p, &ulPostCt);

  switch (rc) {
    case ERROR_ALREADY_RESET:
      rc = NO_ERROR;
  }
  return rc;
}

WRes ManualResetEvent_Create(CManualResetEvent *p, int signaled) {
  return DosCreateEventSem(NULL, p, 0, signaled);
}

WRes ManualResetEvent_CreateNotSignaled(CManualResetEvent *p) {
  return ManualResetEvent_Create(p, 0);
}

WRes AutoResetEvent_Create(CAutoResetEvent *p, int signaled) {
  return DosCreateEventSem(NULL, p, DCE_AUTORESET, signaled);
}

WRes AutoResetEvent_CreateNotSignaled(CAutoResetEvent *p) {
  return AutoResetEvent_Create(p, 0);
}

WRes Semaphore_Create(CSemaphore *p, UInt32 initCount, UInt32 maxCount)
{
  if (initCount > 1 || maxCount > 1) {
    OS2Abort("Semaphore_Create: unsupported initCount or maxCount value");
  }
  return DosCreateEventSem(NULL, p, DCE_POSTONE, initCount > 0);
}

WRes Semaphore_Release1(CSemaphore *p) {
  return Event_Set(p);
}

WRes Semaphore_ReleaseN(CSemaphore *p, UInt32 num)
{
  if (num > 1) {
    OS2Abort("Semaphore_ReleaseN: unsupported num value");
  }
  return Event_Set(p);
}

DWORD WaitForMultipleObjects(DWORD count, const HANDLE *handles, BOOL wait_all, DWORD timeout)
{
  PSEMRECORD pSemRecs = (PSEMRECORD)calloc(sizeof(SEMRECORD), count);
  DWORD i;
  HMUX  hmux;
  ULONG ulUser;

  for (i = 0; i < count; i++) {
    pSemRecs[i].hsemCur = (HSEM)handles[i];
    pSemRecs[i].ulUser = i;
  }

  if((errno = DosCreateMuxWaitSem(NULL, &hmux, count, pSemRecs,
                                  wait_all ? DCMW_WAIT_ALL : DCMW_WAIT_ANY)) == NO_ERROR)
  {
    errno = DosWaitMuxWaitSem(hmux, timeout, &ulUser);
    DosCloseMuxWaitSem(hmux);
  }
  if(errno) {
    return 0xFFFFFFFF;
  } else {
    return WAIT_OBJECT_0 + ulUser;
  }
}

#else /* __OS2__ */

static WRes GetError()
{
  DWORD res = GetLastError();
  return (res) ? (WRes)(res) : 1;
}

WRes HandleToWRes(HANDLE h) { return (h != 0) ? 0 : GetError(); }
WRes BOOLToWRes(BOOL v) { return v ? 0 : GetError(); }

WRes HandlePtr_Close(HANDLE *p)
{
  if (*p != NULL)
    if (!CloseHandle(*p))
      return GetError();
  *p = NULL;
  return 0;
}

WRes Handle_WaitObject(HANDLE h) { return (WRes)WaitForSingleObject(h, INFINITE); }

WRes Thread_Create(CThread *p, THREAD_FUNC_TYPE func, LPVOID param)
{
  unsigned threadId; /* Windows Me/98/95: threadId parameter may not be NULL in _beginthreadex/CreateThread functions */
  *p =
    #ifdef UNDER_CE
    CreateThread(0, 0, func, param, 0, &threadId);
    #else
    (HANDLE)_beginthreadex(NULL, 0, func, param, 0, &threadId);
    #endif
    /* maybe we must use errno here, but probably GetLastError() is also OK. */
  return HandleToWRes(*p);
}

WRes Event_Create(CEvent *p, BOOL manualReset, int signaled)
{
  *p = CreateEvent(NULL, manualReset, (signaled ? TRUE : FALSE), NULL);
  return HandleToWRes(*p);
}

WRes Event_Set(CEvent *p) { return BOOLToWRes(SetEvent(*p)); }
WRes Event_Reset(CEvent *p) { return BOOLToWRes(ResetEvent(*p)); }

WRes ManualResetEvent_Create(CManualResetEvent *p, int signaled) { return Event_Create(p, TRUE, signaled); }
WRes AutoResetEvent_Create(CAutoResetEvent *p, int signaled) { return Event_Create(p, FALSE, signaled); }
WRes ManualResetEvent_CreateNotSignaled(CManualResetEvent *p) { return ManualResetEvent_Create(p, 0); }
WRes AutoResetEvent_CreateNotSignaled(CAutoResetEvent *p) { return AutoResetEvent_Create(p, 0); }


WRes Semaphore_Create(CSemaphore *p, UInt32 initCount, UInt32 maxCount)
{
  *p = CreateSemaphore(NULL, (LONG)initCount, (LONG)maxCount, NULL);
  return HandleToWRes(*p);
}

static WRes Semaphore_Release(CSemaphore *p, LONG releaseCount, LONG *previousCount)
  { return BOOLToWRes(ReleaseSemaphore(*p, releaseCount, previousCount)); }
WRes Semaphore_ReleaseN(CSemaphore *p, UInt32 num)
  { return Semaphore_Release(p, (LONG)num, NULL); }
WRes Semaphore_Release1(CSemaphore *p) { return Semaphore_ReleaseN(p, 1); }

WRes CriticalSection_Init(CCriticalSection *p)
{
  /* InitializeCriticalSection can raise only STATUS_NO_MEMORY exception */
  #ifdef _MSC_VER
  __try
  #endif
  {
    InitializeCriticalSection(p);
    /* InitializeCriticalSectionAndSpinCount(p, 0); */
  }
  #ifdef _MSC_VER
  __except (EXCEPTION_EXECUTE_HANDLER) { return 1; }
  #endif
  return 0;
}

#endif /* __OS2__ */
