// Windows/FileDir.cpp

#include "StdAfx.h"

#ifndef _UNICODE
#include "../Common/StringConvert.h"
#endif

#include "FileDir.h"
#include "FileFind.h"
#include "FileName.h"
#include "Time.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {
namespace NFile {

// SetCurrentDirectory doesn't support \\?\ prefix

#ifdef WIN_LONG_PATH
bool GetLongPathBase(CFSTR fileName, UString &res);
bool GetLongPath(CFSTR fileName, UString &res);
#endif

namespace NDirectory {

#if defined(_WIN32) && !defined(UNDER_CE)

bool MyGetWindowsDirectory(FString &path)
{
  UINT needLength;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    TCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetWindowsDirectory(s, MAX_PATH + 1);
    path = fas2fs(s);
  }
  else
  #endif
  {
    WCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetWindowsDirectoryW(s, MAX_PATH + 1);
    path = us2fs(s);
  }
  return (needLength > 0 && needLength <= MAX_PATH);
}


bool MyGetSystemDirectory(FString &path)
{
  UINT needLength;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    TCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetSystemDirectory(s, MAX_PATH + 1);
    path = fas2fs(s);
  }
  else
  #endif
  {
    WCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetSystemDirectoryW(s, MAX_PATH + 1);
    path = us2fs(s);
  }
  return (needLength > 0 && needLength <= MAX_PATH);
}
#endif

bool SetDirTime(CFSTR fileName, const FILETIME *cTime, const FILETIME *aTime, const FILETIME *mTime)
{
  #ifdef __OS2__
    FILESTATUS3 fsts3PathInfo = {{0}};
    ULONG ulBufferSize  = sizeof(FILESTATUS3);
    APIRET rc;

    if ((rc = DosQueryPathInfo(fileName, FIL_STANDARD, &fsts3PathInfo, ulBufferSize)) == NO_ERROR)
    {
      if (cTime) NTime::FileTimeToDosTime(*cTime, *((UInt32*)&fsts3PathInfo.fdateCreation));
      if (aTime) NTime::FileTimeToDosTime(*aTime, *((UInt32*)&fsts3PathInfo.fdateLastAccess));
      if (mTime) NTime::FileTimeToDosTime(*mTime, *((UInt32*)&fsts3PathInfo.fdateLastWrite));

      if ((rc = DosSetPathInfo(fileName, FIL_STANDARD, &fsts3PathInfo,
                                  ulBufferSize, DSPI_WRTTHRU)) == NO_ERROR)
      {
        return true;
      }
    }
    errno = rc;
    return false;
  #else
    #ifndef _UNICODE
    if (!g_IsNT)
    {
      ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
      return false;
    }
    #endif
    HANDLE hDir = ::CreateFileW(fs2us(fileName), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    #ifdef WIN_LONG_PATH
    if (hDir == INVALID_HANDLE_VALUE)
    {
      UString longPath;
      if (GetLongPath(fileName, longPath))
        hDir = ::CreateFileW(longPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    }
    #endif

    bool res = false;
    if (hDir != INVALID_HANDLE_VALUE)
    {
      res = BOOLToBool(::SetFileTime(hDir, cTime, aTime, mTime));
      ::CloseHandle(hDir);
    }
    return res;
  #endif
}

#ifdef WIN_LONG_PATH
bool GetLongPaths(CFSTR s1, CFSTR s2, UString &d1, UString &d2)
{
  if (!GetLongPathBase(s1, d1) ||
      !GetLongPathBase(s2, d2))
    return false;
  if (d1.IsEmpty() && d2.IsEmpty())
    return false;
  if (d1.IsEmpty()) d1 = fs2us(s1);
  if (d2.IsEmpty()) d2 = fs2us(s2);
  return true;
}
#endif

bool MySetFileAttributes(CFSTR fileName, DWORD fileAttributes)
{
  #ifdef __OS2__
    FILESTATUS3 fsts3PathInfo = {{0}};
    ULONG ulBufferSize  = sizeof(FILESTATUS3);
    APIRET rc;

    if ((rc = DosQueryPathInfo(fileName, FIL_STANDARD, &fsts3PathInfo, ulBufferSize)) == NO_ERROR)
    {
      fsts3PathInfo.attrFile = fileAttributes;
      if ((rc = DosSetPathInfo(fileName, FIL_STANDARD, &fsts3PathInfo,
                               ulBufferSize, DSPI_WRTTHRU)) == NO_ERROR)
      {
        return true;
      }
    }
    errno = rc;
    return false;
  #else
    #ifndef _UNICODE
    if (!g_IsNT)
    {
      if (::SetFileAttributes(fs2fas(fileName), fileAttributes))
        return true;
    }
    else
    #endif
    {
      if (::SetFileAttributesW(fs2us(fileName), fileAttributes))
        return true;
      #ifdef WIN_LONG_PATH
      UString longPath;
      if (GetLongPath(fileName, longPath))
        return BOOLToBool(::SetFileAttributesW(longPath, fileAttributes));
      #endif
    }
    return false;
  #endif
}

bool MyRemoveDirectory(CFSTR path)
{
  #ifdef __OS2__
    APIRET rc = DosDeleteDir(path);
    if (rc == NO_ERROR) {
      return true;
    } else {
      errno = rc;
      return false;
    }
  #else
    #ifndef _UNICODE
    if (!g_IsNT)
    {
      if (::RemoveDirectory(fs2fas(path)))
        return true;
    }
    else
    #endif
    {
      if (::RemoveDirectoryW(fs2us(path)))
        return true;
      #ifdef WIN_LONG_PATH
      UString longPath;
      if (GetLongPath(path, longPath))
        return BOOLToBool(::RemoveDirectoryW(longPath));
      #endif
    }
    return false;
  #endif
}

bool MyMoveFile(CFSTR existFileName, CFSTR newFileName)
{
  #ifdef __OS2__
    APIRET rc = DosMove(existFileName, newFileName);
    if (rc == NO_ERROR) {
      return true;
    } else {
      errno = rc;
      return false;
    }
  #else
    #ifndef _UNICODE
    if (!g_IsNT)
    {
      if (::MoveFile(fs2fas(existFileName), fs2fas(newFileName)))
        return true;
    }
    else
    #endif
    {
      if (::MoveFileW(fs2us(existFileName), fs2us(newFileName)))
        return true;
      #ifdef WIN_LONG_PATH
      UString d1, d2;
      if (GetLongPaths(existFileName, newFileName, d1, d2))
        return BOOLToBool(::MoveFileW(d1, d2));
      #endif
    }
    return false;
  #endif
}

bool MyCreateDirectory(CFSTR path)
{
  #ifdef __OS2__
    APIRET rc = DosCreateDir(path, 0);
    if (rc == NO_ERROR) {
      return true;
    } else if (rc == ERROR_ACCESS_DENIED) {
      rc = ERROR_ALREADY_EXISTS;
    }
    errno = rc;
    return false;
  #else
    #ifndef _UNICODE
    if (!g_IsNT)
    {
      if (::CreateDirectory(fs2fas(path), NULL))
        return true;
    }
    else
    #endif
    {
      if (::CreateDirectoryW(fs2us(path), NULL))
        return true;
      #ifdef WIN_LONG_PATH
      if (::GetLastError() != ERROR_ALREADY_EXISTS)
      {
        UString longPath;
        if (GetLongPath(path, longPath))
          return BOOLToBool(::CreateDirectoryW(longPath, NULL));
      }
      #endif
    }
    return false;
  #endif
}

bool CreateComplexDirectory(CFSTR _aPathName)
{
  FString pathName = _aPathName;
  int pos = pathName.ReverseFind(FCHAR_PATH_SEPARATOR);
  if (pos > 0 && pos == pathName.Length() - 1)
  {
    if (pathName.Length() == 3 && pathName[1] == L':')
      return true; // Disk folder;
    pathName.Delete(pos);
  }
  FString pathName2 = pathName;
  pos = pathName.Length();
  for (;;)
  {
    if (MyCreateDirectory(pathName))
      break;
    if (::GetLastError() == ERROR_ALREADY_EXISTS)
    {
      NFind::CFileInfo fileInfo;
      if (!fileInfo.Find(pathName)) // For network folders
        return true;
      if (!fileInfo.IsDir())
        return false;
      break;
    }
    pos = pathName.ReverseFind(FCHAR_PATH_SEPARATOR);
    if (pos < 0 || pos == 0)
      return false;
    if (pathName[pos - 1] == L':')
      return false;
    pathName = pathName.Left(pos);
  }
  pathName = pathName2;
  while (pos < pathName.Length())
  {
    pos = pathName.Find(FCHAR_PATH_SEPARATOR, pos + 1);
    if (pos < 0)
      pos = pathName.Length();
    if (!MyCreateDirectory(pathName.Left(pos)))
      return false;
  }
  return true;
}

bool DeleteFileAlways(CFSTR name)
{
  if (!MySetFileAttributes(name, 0))
    return false;
  #ifdef __OS2__
    APIRET rc = DosDelete(name);
    if (rc == NO_ERROR) {
      return true;
    }
    errno = rc;
    return false;
  #else
    #ifndef _UNICODE
    if (!g_IsNT)
    {
      if (::DeleteFile(fs2fas(name)))
        return true;
    }
    else
    #endif
    {
      if (::DeleteFileW(fs2us(name)))
        return true;
      #ifdef WIN_LONG_PATH
      UString longPath;
      if (GetLongPath(name, longPath))
        return BOOLToBool(::DeleteFileW(longPath));
      #endif
    }
    return false;
  #endif
}

static bool RemoveDirectorySubItems2(const FString pathPrefix, const NFind::CFileInfo &fileInfo)
{
  if (fileInfo.IsDir())
    return RemoveDirectoryWithSubItems(pathPrefix + fileInfo.Name);
  return DeleteFileAlways(pathPrefix + fileInfo.Name);
}
bool RemoveDirectoryWithSubItems(const FString &path)
{
  NFind::CFileInfo fileInfo;
  FString pathPrefix = path + FCHAR_PATH_SEPARATOR;
  {
    NFind::CEnumerator enumerator(pathPrefix + FCHAR_ANY_MASK);
    while (enumerator.Next(fileInfo))
      if (!RemoveDirectorySubItems2(pathPrefix, fileInfo))
        return false;
  }
  if (!MySetFileAttributes(path, 0))
    return false;
  return MyRemoveDirectory(path);
}

#ifdef UNDER_CE

bool MyGetFullPathName(CFSTR fileName, FString &resFullPath)
{
  resFullPath = fileName;
  return true;
}

#else

#ifdef WIN_LONG_PATH

static FString GetLastPart(CFSTR path)
{
  int i = MyStringLen(path);
  for (; i > 0; i--)
  {
    FChar c = path[i - 1];
    if (c == FCHAR_PATH_SEPARATOR || c == '/')
      break;
  }
  return path + i;
}

static void AddTrailingDots(CFSTR oldPath, FString &newPath)
{
  int len = MyStringLen(oldPath);
  int i;
  for (i = len; i > 0 && oldPath[i - 1] == '.'; i--);
  if (i == 0 || i == len)
    return;
  FString oldName = GetLastPart(oldPath);
  FString newName = GetLastPart(newPath);
  int nonDotsLen = oldName.Length() - (len - i);
  if (nonDotsLen == 0 || newName.CompareNoCase(oldName.Left(nonDotsLen)) != 0)
    return;
  for (; i != len; i++)
    newPath += '.';
}

#endif

bool MyGetFullPathName(CFSTR fileName, FString &resFullPath)
{
  resFullPath.Empty();
  #ifdef __OS2__
    CHAR   chFullName[CCHMAXPATH];
    UCHAR  chDirName[CCHMAXPATH] = "X:\\";
    ULONG  cbLen = CCHMAXPATH-3;
    ULONG  ulCurDisk;
    ULONG  ulDiskMap;
    APIRET rc;

    if ((rc = DosQueryCurrentDisk(&ulCurDisk, &ulDiskMap)) == NO_ERROR) {
      chDirName[0] = ulCurDisk + 'A' - 1;
      if ((rc = DosQueryCurrentDir(0, chDirName+3, &cbLen)) == NO_ERROR) {
        FOCPathRelToAbs((char*)chDirName, fileName, chFullName, sizeof(chFullName));
        resFullPath = fas2fs(chFullName);
        return true;
      }
    }
    errno = rc;
    return false;
  #else
    #ifndef _UNICODE
    if (!g_IsNT)
    {
      TCHAR s[MAX_PATH + 2];
      s[0] = 0;
      LPTSTR fileNamePointer = 0;
      DWORD needLength = ::GetFullPathName(fs2fas(fileName), MAX_PATH + 1, s, &fileNamePointer);
      if (needLength == 0 || needLength > MAX_PATH)
        return false;
      resFullPath = fas2fs(s);
      return true;
    }
    else
    #endif
    {
      LPWSTR fileNamePointer = 0;
      WCHAR s[MAX_PATH + 2];
      s[0] = 0;
      DWORD needLength = ::GetFullPathNameW(fs2us(fileName), MAX_PATH + 1, s, &fileNamePointer);
      if (needLength == 0)
        return false;
      if (needLength <= MAX_PATH)
      {
        resFullPath = us2fs(s);
        return true;
      }
      #ifdef WIN_LONG_PATH
      needLength++;
      UString temp;
      LPWSTR buffer = temp.GetBuffer(needLength + 1);
      buffer[0] = 0;
      DWORD needLength2 = ::GetFullPathNameW(fs2us(fileName), needLength, buffer, &fileNamePointer);
      temp.ReleaseBuffer();
      if (needLength2 > 0 && needLength2 <= needLength)
      {
        resFullPath = us2fs(temp);
        AddTrailingDots(fileName, resFullPath);
        return true;
      }
      #endif
      return false;
    }
  #endif
}

bool MySetCurrentDirectory(CFSTR path)
{
  #ifdef __OS2__
    APIRET rc = DosSetCurrentDir(path);
    if (rc == NO_ERROR) {
      return true;
    }
    errno = rc;
    return false;
  #else
    #ifndef _UNICODE
    if (!g_IsNT)
    {
      return BOOLToBool(::SetCurrentDirectory(fs2fas(path)));
    }
    else
    #endif
    {
      return BOOLToBool(::SetCurrentDirectoryW(fs2us(path)));
    }
  #endif
}

bool MyGetCurrentDirectory(FString &path)
{
  path.Empty();
  #ifdef __OS2__
    UCHAR  chDirName[CCHMAXPATH];
    ULONG  cbLen = CCHMAXPATH;
    APIRET rc;

    if ((rc = DosQueryCurrentDir(0, chDirName, &cbLen)) == NO_ERROR) {
      path = fas2fs((char*)chDirName);
      return true;
    }
    errno = rc;
    return false;
  #else
    DWORD needLength;
    #ifndef _UNICODE
    if (!g_IsNT)
    {
      TCHAR s[MAX_PATH + 2];
      s[0] = 0;
      needLength = ::GetCurrentDirectory(MAX_PATH + 1, s);
      path = fas2fs(s);
    }
    else
    #endif
    {
      WCHAR s[MAX_PATH + 2];
      s[0] = 0;
      needLength = ::GetCurrentDirectoryW(MAX_PATH + 1, s);
      path = us2fs(s);
    }
    return (needLength > 0 && needLength <= MAX_PATH);
  #endif
}

#endif

bool GetFullPathAndSplit(CFSTR path, FString &resDirPrefix, FString &resFileName)
{
  bool res = MyGetFullPathName(path, resFileName);
  if (!res)
    resFileName = path;
  int pos = resFileName.ReverseFind(FCHAR_PATH_SEPARATOR);
  if (pos >= 0)
  {
    resDirPrefix = resFileName.Left(pos + 1);
    resFileName = resFileName.Mid(pos + 1);
  }
  return res;
}

bool GetOnlyDirPrefix(CFSTR path, FString &resDirPrefix)
{
  FString resFileName;
  return GetFullPathAndSplit(path, resDirPrefix, resFileName);
}

bool MyGetTempPath(FString &path)
{
  path.Empty();
  #ifdef __OS2__
    PSZ    pszTempPath;
    CHAR   chFullPath[CCHMAXPATH];
    APIRET rc;

    if ((rc = DosScanEnv("TEMP", &pszTempPath)) == NO_ERROR) {
      strlcpy(chFullPath, pszTempPath, CCHMAXPATH);
      if (chFullPath[strlen(chFullPath)-1] != '\\') {
        strlcat(chFullPath, "\\", CCHMAXPATH);
      }
      return true;
    }
    errno = rc;
    return false;
  #else
    DWORD needLength;
    #ifndef _UNICODE
    if (!g_IsNT)
    {
      TCHAR s[MAX_PATH + 2];
      s[0] = 0;
      needLength = ::GetTempPath(MAX_PATH + 1, s);
      path = fas2fs(s);
    }
    else
    #endif
    {
      WCHAR s[MAX_PATH + 2];
      s[0] = 0;
      needLength = ::GetTempPathW(MAX_PATH + 1, s);;
      path = us2fs(s);
    }
    return (needLength > 0 && needLength <= MAX_PATH);
  #endif
}

static bool CreateTempFile(CFSTR prefix, bool addRandom, FString &path, NIO::COutFile *outFile)
{
  #ifdef __OS2__
    char     filename[CCHMAXPATH];
    LONGLONG ordstart;
    LONGLONG ordfname;
    HFILE    hfile;
    ULONG    action;
    APIRET   rc;

    ordfname = ((getpid() & 0xFFFFULL) << 28) | (clock() & 0xFFFFFFFUL);
    ordstart = ordfname;

    for (;;) {
      snprintf(filename, sizeof(filename ), "%s%08llX.%03llX",
               prefix, (ordfname & 0xFFFFFFFF000ULL) >> 12, ordfname & 0xFFFUL);

      rc = DosOpen(filename, &hfile, &action, 0, FILE_NORMAL,
                   OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_FAIL_IF_EXISTS,
                   OPEN_FLAGS_FAIL_ON_ERROR  | OPEN_FLAGS_NOINHERIT |
                   OPEN_SHARE_DENYREADWRITE  | OPEN_ACCESS_WRITEONLY, NULL);

      if (rc == NO_ERROR) {
        DosClose(hfile);
        break;
      } else if (rc == ERROR_OPEN_FAILED) {
        ordfname = (ordfname & 0xFFFF0000000ULL) | ((ordfname + 1) & 0x0000FFFFFFFULL);
        if (ordfname == ordstart) {
          break;
        }
      } else {
        break;
      }
    }
    if (rc == NO_ERROR) {
      return true;
    }
    errno = rc;
    return false;
  #else
    UInt32 d = (GetTickCount() << 12) ^ (GetCurrentThreadId() << 14) ^ GetCurrentProcessId();
    for (unsigned i = 0; i < 100; i++)
    {
      path = prefix;
      if (addRandom)
      {
        FChar s[16];
        UInt32 value = d;
        unsigned k;
        for (k = 0; k < 8; k++)
        {
          unsigned t = value & 0xF;
          value >>= 4;
          s[k] = (char)((t < 10) ? ('0' + t) : ('A' + (t - 10)));
        }
        s[k] = '\0';
        if (outFile)
          path += FChar('.');
        path += s;
        UInt32 step = GetTickCount() + 2;
        if (step == 0)
          step = 1;
        d += step;
      }
      addRandom = true;
      if (outFile)
        path += FTEXT(".tmp");
      if (NFind::DoesFileOrDirExist(path))
      {
        SetLastError(ERROR_ALREADY_EXISTS);
        continue;
      }
      if (outFile)
      {
        if (outFile->Create(path, false))
          return true;
      }
      else
      {
        if (MyCreateDirectory(path))
          return true;
      }
      DWORD error = GetLastError();
      if (error != ERROR_FILE_EXISTS &&
          error != ERROR_ALREADY_EXISTS)
        break;
    }
    path.Empty();
    return false;
  #endif
}

bool CTempFile::Create(CFSTR prefix, NIO::COutFile *outFile)
{
  if (!Remove())
    return false;
  if (!CreateTempFile(prefix, false, _path, outFile))
    return false;
  _mustBeDeleted = true;
  return true;
}

bool CTempFile::CreateRandomInTempFolder(CFSTR namePrefix, NIO::COutFile *outFile)
{
  if (!Remove())
    return false;
  FString tempPath;
  if (!MyGetTempPath(tempPath))
    return false;
  if (!CreateTempFile(tempPath + namePrefix, true, _path, outFile))
    return false;
  _mustBeDeleted = true;
  return true;
}

bool CTempFile::Remove()
{
  if (!_mustBeDeleted)
    return true;
  _mustBeDeleted = !DeleteFileAlways(_path);
  return !_mustBeDeleted;
}

bool CTempFile::MoveTo(CFSTR name, bool deleteDestBefore)
{
  if (deleteDestBefore)
    if (NFind::DoesFileExist(name))
      if (!DeleteFileAlways(name))
        return false;
  DisableDeleting();
  return MyMoveFile(_path, name);
}

bool CTempDir::Create(CFSTR prefix)
{
  if (!Remove())
    return false;
  FString tempPath;
  if (!MyGetTempPath(tempPath))
    return false;
  if (!CreateTempFile(tempPath + prefix, true, _path, NULL))
    return false;
  _mustBeDeleted = true;
  return true;
}

bool CTempDir::Remove()
{
  if (!_mustBeDeleted)
    return true;
  _mustBeDeleted = !RemoveDirectoryWithSubItems(_path);
  return !_mustBeDeleted;
}

}}}
