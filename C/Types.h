/* Types.h -- Basic types
2010-10-09 : Igor Pavlov : Public domain */

#ifndef __7Z_TYPES_H
#define __7Z_TYPES_H

#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __OS2__
#define INCL_BASE
#define INCL_PM
#define INCL_LONGLONG
#include <os2.h>
#include <process.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <sys/timeb.h>
#include <stdlib.h>
#include <uconv.h>
#include <foc.h>

#define _LZMA_UINT32_IS_ULONG

#define LPVOID  PVOID
#define HANDLE  LHANDLE

#define HRESULT LONG
#define S_OK ((HRESULT)0x00000000L)
#define CLASS_E_CLASSNOTAVAILABLE ((HRESULT)0x80040111L)

#ifdef  __MY_WINDOWS_H
#undef  CHAR
#undef  SHORT
#undef  INT
#undef  LONG
#else
typedef ULONG DWORD;
#endif

#define WAIT_OBJECT_0 0x00000000
#define INFINITE -1

#define INVALID_HANDLE_VALUE -1
#define MAX_PATH CCHMAXPATH

#define FILE_ATTRIBUTE_DIRECTORY     FILE_DIRECTORY
#define FILE_ATTRIBUTE_READONLY      FILE_READONLY
#define FILE_ATTRIBUTE_HIDDEN        FILE_HIDDEN
#define FILE_ATTRIBUTE_SYSTEM        FILE_SYSTEM
#define FILE_ATTRIBUTE_ARCHIVE       FILE_ARCHIVED
#define FILE_ATTRIBUTE_COMPRESSED    0
#define FILE_ATTRIBUTE_ENCRYPTED     0
#define FILE_ATTRIBUTE_NORMAL        FILE_NORMAL
#define FILE_ATTRIBUTE_OFFLINE       0
#define FILE_ATTRIBUTE_REPARSE_POINT 0
#define FILE_ATTRIBUTE_SPARSE_FILE   0
#define FILE_ATTRIBUTE_TEMPORARY     0

#define CREATE_NEW        (OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_FAIL_IF_EXISTS)
#define CREATE_ALWAYS     (OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS)
#define OPEN_EXISTING     (OPEN_ACTION_FAIL_IF_NEW   | OPEN_ACTION_OPEN_IF_EXISTS)
#define OPEN_ALWAYS       (OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS)
#define TRUNCATE_EXISTING (OPEN_ACTION_FAIL_IF_NEW   | OPEN_ACTION_REPLACE_IF_EXISTS)

#define GENERIC_READ      1
#define GENERIC_WRITE     2
#define FILE_SHARE_READ   1
#define FILE_SHARE_WRITE  2

#define DOSTIMEFROMOS2(date,time) *((UInt16*)&date) << 16 | *((UInt16*)&time)

#endif /* __OS2__ */

#ifndef EXTERN_C_BEGIN
#ifdef __cplusplus
#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif
#endif

EXTERN_C_BEGIN

#define SZ_OK 0

#define SZ_ERROR_DATA 1
#define SZ_ERROR_MEM 2
#define SZ_ERROR_CRC 3
#define SZ_ERROR_UNSUPPORTED 4
#define SZ_ERROR_PARAM 5
#define SZ_ERROR_INPUT_EOF 6
#define SZ_ERROR_OUTPUT_EOF 7
#define SZ_ERROR_READ 8
#define SZ_ERROR_WRITE 9
#define SZ_ERROR_PROGRESS 10
#define SZ_ERROR_FAIL 11
#define SZ_ERROR_THREAD 12

#define SZ_ERROR_ARCHIVE 16
#define SZ_ERROR_NO_ARCHIVE 17

typedef int SRes;

#ifdef _WIN32
typedef DWORD WRes;
#else
typedef int WRes;
#endif

#ifndef RINOK
#ifndef DEBUG
#define RINOK(x) { int __result__ = (x); if (__result__ != 0) return __result__; }
#else
#define RINOK(x) { int __result__ = (x); if (__result__ != 0) { fprintf(stderr, "%s [%d] %s, result=%d\n", __FILE__, __LINE__, __FUNCTION__, __result__); fflush(stderr); return __result__; }}
#endif
#endif

typedef unsigned char Byte;
typedef short Int16;
typedef unsigned short UInt16;

#ifdef _LZMA_UINT32_IS_ULONG
typedef long Int32;
typedef unsigned long UInt32;
#else
typedef int Int32;
typedef unsigned int UInt32;
#endif

#ifdef _SZ_NO_INT_64

/* define _SZ_NO_INT_64, if your compiler doesn't support 64-bit integers.
   NOTES: Some code will work incorrectly in that case! */

typedef long Int64;
typedef unsigned long UInt64;

#else

#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64 Int64;
typedef unsigned __int64 UInt64;
#define UINT64_CONST(n) n
#else
typedef long long int Int64;
typedef unsigned long long int UInt64;
#define UINT64_CONST(n) n ## ULL
#endif

#endif

#ifdef _LZMA_NO_SYSTEM_SIZE_T
typedef UInt32 SizeT;
#else
typedef size_t SizeT;
#endif

typedef int Bool;
#define True 1
#define False 0


#ifdef _WIN32
#define MY_STD_CALL __stdcall
#elif defined(__OS2__)
#define MY_STD_CALL _System
#define STDAPI extern "C" int MY_STD_CALL
#else
#define MY_STD_CALL
#endif

#ifdef _MSC_VER

#if _MSC_VER >= 1300
#define MY_NO_INLINE __declspec(noinline)
#else
#define MY_NO_INLINE
#endif

#define MY_CDECL __cdecl
#define MY_FAST_CALL __fastcall

#else

#define MY_CDECL
#define MY_FAST_CALL

#endif


/* The following interfaces use first parameter as pointer to structure */

typedef struct
{
  Byte (*Read)(void *p); /* reads one byte, returns 0 in case of EOF or error */
} IByteIn;

typedef struct
{
  void (*Write)(void *p, Byte b);
} IByteOut;

typedef struct
{
  SRes (*Read)(void *p, void *buf, size_t *size);
    /* if (input(*size) != 0 && output(*size) == 0) means end_of_stream.
       (output(*size) < input(*size)) is allowed */
} ISeqInStream;

/* it can return SZ_ERROR_INPUT_EOF */
SRes SeqInStream_Read(ISeqInStream *stream, void *buf, size_t size);
SRes SeqInStream_Read2(ISeqInStream *stream, void *buf, size_t size, SRes errorType);
SRes SeqInStream_ReadByte(ISeqInStream *stream, Byte *buf);

typedef struct
{
  size_t (*Write)(void *p, const void *buf, size_t size);
    /* Returns: result - the number of actually written bytes.
       (result < size) means error */
} ISeqOutStream;

typedef enum
{
  SZ_SEEK_SET = 0,
  SZ_SEEK_CUR = 1,
  SZ_SEEK_END = 2
} ESzSeek;

typedef struct
{
  SRes (*Read)(void *p, void *buf, size_t *size);  /* same as ISeqInStream::Read */
  SRes (*Seek)(void *p, Int64 *pos, ESzSeek origin);
} ISeekInStream;

typedef struct
{
  SRes (*Look)(void *p, const void **buf, size_t *size);
    /* if (input(*size) != 0 && output(*size) == 0) means end_of_stream.
       (output(*size) > input(*size)) is not allowed
       (output(*size) < input(*size)) is allowed */
  SRes (*Skip)(void *p, size_t offset);
    /* offset must be <= output(*size) of Look */

  SRes (*Read)(void *p, void *buf, size_t *size);
    /* reads directly (without buffer). It's same as ISeqInStream::Read */
  SRes (*Seek)(void *p, Int64 *pos, ESzSeek origin);
} ILookInStream;

SRes LookInStream_LookRead(ILookInStream *stream, void *buf, size_t *size);
SRes LookInStream_SeekTo(ILookInStream *stream, UInt64 offset);

/* reads via ILookInStream::Read */
SRes LookInStream_Read2(ILookInStream *stream, void *buf, size_t size, SRes errorType);
SRes LookInStream_Read(ILookInStream *stream, void *buf, size_t size);

#define LookToRead_BUF_SIZE (1 << 14)

typedef struct
{
  ILookInStream s;
  ISeekInStream *realStream;
  size_t pos;
  size_t size;
  Byte buf[LookToRead_BUF_SIZE];
} CLookToRead;

void LookToRead_CreateVTable(CLookToRead *p, int lookahead);
void LookToRead_Init(CLookToRead *p);

typedef struct
{
  ISeqInStream s;
  ILookInStream *realStream;
} CSecToLook;

void SecToLook_CreateVTable(CSecToLook *p);

typedef struct
{
  ISeqInStream s;
  ILookInStream *realStream;
} CSecToRead;

void SecToRead_CreateVTable(CSecToRead *p);

typedef struct
{
  SRes (*Progress)(void *p, UInt64 inSize, UInt64 outSize);
    /* Returns: result. (result != SZ_OK) means break.
       Value (UInt64)(Int64)-1 for size means unknown value. */
} ICompressProgress;

typedef struct
{
  void *(*Alloc)(void *p, size_t size);
  void (*Free)(void *p, void *address); /* address can be 0 */
} ISzAlloc;

#define IAlloc_Alloc(p, size) (p)->Alloc((p), size)
#define IAlloc_Free(p, a) (p)->Free((p), a)

#if defined(_WIN32) || defined(__OS2__)

#define CHAR_PATH_SEPARATOR '\\'
#define WCHAR_PATH_SEPARATOR L'\\'
#define STRING_PATH_SEPARATOR "\\"
#define WSTRING_PATH_SEPARATOR L"\\"

#else

#define CHAR_PATH_SEPARATOR '/'
#define WCHAR_PATH_SEPARATOR L'/'
#define STRING_PATH_SEPARATOR "/"
#define WSTRING_PATH_SEPARATOR L"/"

#endif

EXTERN_C_END

#endif
