/* Threads.h -- multithreading library
2009-03-27 : Igor Pavlov : Public domain */

#ifndef __7Z_THREADS_H
#define __7Z_THREADS_H

#include "Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __OS2__

typedef TID CThread;
#define Thread_Construct(p) *(p) = 0
#define Thread_WasCreated(p) (*(p) != 0)
#define Thread_Close(p) *(p) = 0
#define Thread_Wait(p) DosWaitThread(p, DCWW_WAIT)
typedef unsigned THREAD_FUNC_RET_TYPE;
#define THREAD_FUNC_CALL_TYPE MY_STD_CALL
#define THREAD_FUNC_DECL THREAD_FUNC_RET_TYPE THREAD_FUNC_CALL_TYPE
typedef THREAD_FUNC_RET_TYPE (THREAD_FUNC_CALL_TYPE * THREAD_FUNC_TYPE)(PVOID);
WRes Thread_Create(CThread *p, THREAD_FUNC_TYPE func, PVOID param);

typedef HEV CEvent;
typedef CEvent CAutoResetEvent;
typedef CEvent CManualResetEvent;
#define Event_Construct(p) *(p) = NULLHANDLE
#define Event_IsCreated(p) (*(p) != NULLHANDLE)
#define Event_Close(p) DosCloseEventSem(*(p))
#define Event_Wait(p) DosWaitEventSem(*(p),SEM_INDEFINITE_WAIT)
WRes Event_Set(CEvent *p);
WRes Event_Reset(CEvent *p);
WRes ManualResetEvent_Create(CManualResetEvent *p, int signaled);
WRes ManualResetEvent_CreateNotSignaled(CManualResetEvent *p);
WRes AutoResetEvent_Create(CAutoResetEvent *p, int signaled);
WRes AutoResetEvent_CreateNotSignaled(CAutoResetEvent *p);

typedef HEV CSemaphore;
#define Semaphore_Construct(p) *(p) = NULLHANDLE
#define Semaphore_Close(p) DosCloseEventSem(*(p))
#define Semaphore_Wait(p) DosWaitEventSem(*(p),SEM_INDEFINITE_WAIT)
WRes Semaphore_Create(CSemaphore *p, UInt32 initCount, UInt32 maxCount);
WRes Semaphore_ReleaseN(CSemaphore *p, UInt32 num);
WRes Semaphore_Release1(CSemaphore *p);

typedef HMTX CCriticalSection;
#define CriticalSection_Init(p) DosCreateMutexSem(NULL, p, 0, FALSE)
#define CriticalSection_Delete(p) DosCloseMutexSem(*(p))
#define CriticalSection_Enter(p) DosRequestMutexSem(*(p), SEM_INDEFINITE_WAIT)
#define CriticalSection_Leave(p) DosReleaseMutexSem(*(p))

DWORD WaitForMultipleObjects(DWORD count, const HANDLE *handles, BOOL wait_all, DWORD timeout);
void OS2Abort(const char* message);

#else /* __OS2__ */

WRes HandlePtr_Close(HANDLE *h);
WRes Handle_WaitObject(HANDLE h);

typedef HANDLE CThread;
#define Thread_Construct(p) *(p) = NULL
#define Thread_WasCreated(p) (*(p) != NULL)
#define Thread_Close(p) HandlePtr_Close(p)
#define Thread_Wait(p) Handle_WaitObject(*(p))
typedef unsigned THREAD_FUNC_RET_TYPE;
#define THREAD_FUNC_CALL_TYPE MY_STD_CALL
#define THREAD_FUNC_DECL THREAD_FUNC_RET_TYPE THREAD_FUNC_CALL_TYPE
typedef THREAD_FUNC_RET_TYPE (THREAD_FUNC_CALL_TYPE * THREAD_FUNC_TYPE)(void *);
WRes Thread_Create(CThread *p, THREAD_FUNC_TYPE func, LPVOID param);

typedef HANDLE CEvent;
typedef CEvent CAutoResetEvent;
typedef CEvent CManualResetEvent;
#define Event_Construct(p) *(p) = NULL
#define Event_IsCreated(p) (*(p) != NULL)
#define Event_Close(p) HandlePtr_Close(p)
#define Event_Wait(p) Handle_WaitObject(*(p))
WRes Event_Set(CEvent *p);
WRes Event_Reset(CEvent *p);
WRes ManualResetEvent_Create(CManualResetEvent *p, int signaled);
WRes ManualResetEvent_CreateNotSignaled(CManualResetEvent *p);
WRes AutoResetEvent_Create(CAutoResetEvent *p, int signaled);
WRes AutoResetEvent_CreateNotSignaled(CAutoResetEvent *p);

typedef HANDLE CSemaphore;
#define Semaphore_Construct(p) (*p) = NULL
#define Semaphore_Close(p) HandlePtr_Close(p)
#define Semaphore_Wait(p) Handle_WaitObject(*(p))
WRes Semaphore_Create(CSemaphore *p, UInt32 initCount, UInt32 maxCount);
WRes Semaphore_ReleaseN(CSemaphore *p, UInt32 num);
WRes Semaphore_Release1(CSemaphore *p);

typedef CRITICAL_SECTION CCriticalSection;
WRes CriticalSection_Init(CCriticalSection *p);
#define CriticalSection_Delete(p) DeleteCriticalSection(p)
#define CriticalSection_Enter(p) EnterCriticalSection(p)
#define CriticalSection_Leave(p) LeaveCriticalSection(p)

#endif /* __OS2__ */

#ifdef __cplusplus
}
#endif

#endif
