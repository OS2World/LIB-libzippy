// UefiHandler.cpp

#include "StdAfx.h"

// #define SHOW_DEBUG_INFO

// #include <stdio.h>

#ifdef SHOW_DEBUG_INFO
#include <stdio.h>
#endif

#include "../../../C/7zCrc.h"
#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"
#include "../../../C/LzmaDec.h"

#include "Common/Buffer.h"
#include "Common/ComTry.h"
#include "Common/IntToString.h"
#include "Common/StringConvert.h"

#include "Windows/PropVariant.h"
#include "Windows/PropVariantUtils.h"

#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#include "./Common/FindSignature.h"

#ifdef SHOW_DEBUG_INFO
#define PRF(x) x
#else
#define PRF(x)
#endif

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)
#define Get24(p) (Get32(p) & 0xFFFFFF)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

namespace NArchive {
namespace NUefi {

static const UInt32 kBufTotalSizeMax = (1 << 29);
static const UInt32 kNumFilesMax = (1 << 18);
static const int kLevelMax = 64;

static void *SzAlloc(void *p, size_t size) { p = p; return MyAlloc(size); }
static void SzFree(void *p, void *address) { p = p; MyFree(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

static const UInt32 kFvHeaderSize = 0x38;
static const UInt32 kGuidSize = 16;
static const UInt32 kCapsuleSigSize = kGuidSize;
#define CAPSULE_SIGNATURE \
  { 0xBD,0x86,0x66,0x3B,0x76,0x0D,0x30,0x40,0xB7,0x0E,0xB5,0x51,0x9E,0x2F,0xC5,0xA0 }
static const Byte kCapsuleSig[kCapsuleSigSize] = CAPSULE_SIGNATURE;

static const UInt32 kFfsGuidOffset = 16;
#define FFS_SIGNATURE \
  { 0xD9,0x54,0x93,0x7A,0x68,0x04,0x4A,0x44,0x81,0xCE,0x0B,0xF6,0x17,0xD8,0x90,0xDF }
static const Byte k_FFS_Guid[kGuidSize] = FFS_SIGNATURE;

static const Byte k_MacFS_Guid[kGuidSize] =
  { 0xAD,0xEE,0xAD,0x04,0xFF,0x61,0x31,0x4D,0xB6,0xBA,0x64,0xF8,0xBF,0x90,0x1F,0x5A };

static const UInt32 kFvSignature = 0x4856465F;

static const Byte kGuids[][kGuidSize] =
{
  { 0xB0,0xCD,0x1B,0xFC,0x31,0x7D,0xAA,0x49,0x93,0x6A,0xA4,0x60,0x0D,0x9D,0xD0,0x83 },
  { 0x2E,0x06,0xA0,0x1B,0x79,0xC7,0x82,0x45,0x85,0x66,0x33,0x6A,0xE8,0xF7,0x8F,0x09 },
  { 0x25,0x4E,0x37,0x7E,0x01,0x8E,0xEE,0x4F,0x87,0xf2,0x39,0x0C,0x23,0xC6,0x06,0xCD },
  { 0x97,0xE5,0x1B,0x16,0xC5,0xE9,0xDB,0x49,0xAE,0x50,0xC4,0x62,0xAB,0x54,0xEE,0xDA },
  { 0xDB,0x7F,0xAD,0x77,0x2A,0xDF,0x02,0x43,0x88,0x98,0xC7,0x2E,0x4C,0xDB,0xD0,0xF4 },
  { 0xAB,0x71,0xCF,0xF5,0x4B,0xB0,0x7E,0x4B,0x98,0x8A,0xD8,0xA0,0xD4,0x98,0xE6,0x92 },
  { 0x91,0x45,0x53,0x7A,0xCE,0x37,0x81,0x48,0xB3,0xC9,0x71,0x38,0x14,0xF4,0x5D,0x6B },
  { 0x84,0xE6,0x7A,0x36,0x5D,0x33,0x71,0x46,0xA1,0x6D,0x89,0x9D,0xBF,0xEA,0x6B,0x88 },
  { 0x98,0x07,0x40,0x24,0x07,0x38,0x42,0x4A,0xB4,0x13,0xA1,0xEC,0xEE,0x20,0x5D,0xD8 },
  { 0xEE,0xA2,0x3F,0x28,0x2C,0x53,0x4D,0x48,0x93,0x83,0x9F,0x93,0xB3,0x6F,0x0B,0x7E },
  { 0x9B,0xD5,0xB8,0x98,0xBA,0xE8,0xEE,0x48,0x98,0xDD,0xC2,0x95,0x39,0x2F,0x1E,0xDB },
  { 0x09,0x6D,0xE3,0xC3,0x94,0x82,0x97,0x4B,0xA8,0x57,0xD5,0x28,0x8F,0xE3,0x3E,0x28 },
  { 0x18,0x88,0x53,0x4A,0xE0,0x5A,0xB2,0x4E,0xB2,0xEB,0x48,0x8B,0x23,0x65,0x70,0x22 }
};


static const char *kGuidNames[] =
{
  "CRC",
  "VolumeTopFile",
  "ACPI",
  "ACPI2",
  "Main",
  "Intel32",
  "Intel64",
  "Intel32c",
  "Intel64c",
  "MacVolume",
  "MacUpdate.txt",
  "MacName",
  "Insyde"
};

enum
{
  kGuidIndex_CRC = 0
};

struct CSigExtPair
{
  const char *ext;
  unsigned sigSize;
  Byte sig[16];
};

static const CSigExtPair g_Sigs[] =
{
  { "bmp",  2, { 'B','M' } },
  { "riff", 4, { 'R','I','F','F' } },
  { "pe",   2, { 'M','Z'} },
  { "gif",  6, { 'G','I','F','8','9', 'a' } },
  { "png",  8, { 0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A } },
  { "jpg", 10, { 0xFF,0xD8,0xFF,0xE0,0x00,0x10,0x4A,0x46,0x49,0x46 } },
  { "rom",  2, { 0x55,0xAA } }
};

enum
{
  kSig_BMP,
  kSig_RIFF,
  kSig_PE
};

static const char *FindExt(const Byte *p, size_t size)
{
  unsigned i;
  for (i = 0; i < ARRAY_SIZE(g_Sigs); i++)
  {
    const CSigExtPair &pair = g_Sigs[i];
    if (size >= pair.sigSize)
      if (memcmp(p, pair.sig, pair.sigSize) == 0)
        break;
  }
  if (i == ARRAY_SIZE(g_Sigs))
    return NULL;
  switch (i)
  {
    case kSig_BMP:
      if (GetUi32(p + 2) > size || GetUi32(p + 0xA) > size)
        return NULL;
      break;
    case kSig_RIFF:
      if (GetUi32(p + 8) == 0x45564157 || GetUi32(p + 0xC) == 0x20746D66 )
        return "wav";
      break;
    case kSig_PE:
    {
      if (size < 512)
        return NULL;
      UInt32 peOffset = GetUi32(p + 0x3C);
      if (peOffset >= 0x1000 || peOffset + 512 > size || (peOffset & 7) != 0)
        return NULL;
      if (GetUi32(p + peOffset) != 0x00004550)
        return NULL;
      break;
    }
  }
  return g_Sigs[i].ext;
}

static bool AreGuidsEq(const Byte *p1, const Byte *p2)
{
  return memcmp(p1, p2, kGuidSize) == 0;
}

static int FindGuid(const Byte *p)
{
  for (int i = 0; i < ARRAY_SIZE(kGuids); i++)
    if (AreGuidsEq(p, kGuids[i]))
      return i;
  return -1;
}
 
static bool IsFfs(const Byte *p)
{
  return (Get32(p + 0x28) == kFvSignature && AreGuidsEq(p + kFfsGuidOffset, k_FFS_Guid));
}

#define FVB_ERASE_POLARITY  (1 << 11)

/*
static const CUInt32PCharPair g_FV_Attribs[] =
{
  {  0, "ReadDisabledCap" },
  {  1, "ReadEnabledCap" },
  {  2, "ReadEnabled" },
  {  3, "WriteDisabledCap" },
  {  4, "WriteEnabledCap" },
  {  5, "WriteEnabled" },
  {  6, "LockCap" },
  {  7, "Locked" },

  {  9, "StickyWrite" },
  { 10, "MemoryMapped" },
  { 11, "ErasePolarity" },
  
  { 12, "ReadLockCap" },
  { 13, "WriteLockCap" },
  { 14, "WriteLockCap" }
};
*/

enum
{
  FV_FILETYPE_ALL,
  FV_FILETYPE_RAW,
  FV_FILETYPE_FREEFORM,
  FV_FILETYPE_SECURITY_CORE,
  FV_FILETYPE_PEI_CORE,
  FV_FILETYPE_DXE_CORE,
  FV_FILETYPE_PEIM,
  FV_FILETYPE_DRIVER,
  FV_FILETYPE_COMBINED_PEIM_DRIVER,
  FV_FILETYPE_APPLICATION,
  // The value 0x0A is reserved and should not be used
  FV_FILETYPE_FIRMWARE_VOLUME_IMAGE = 0x0B,
  // types 0xF0 - 0xFF are FFS file types
  FV_FILETYPE_FFS_PAD = 0xF0
};

static const char *g_FileTypes[] =
{
  "ALL",
  "RAW",
  "FREEFORM",
  "SECURITY_CORE",
  "PEI_CORE",
  "DXE_CORE",
  "PEIM",
  "DRIVER",
  "COMBINED_PEIM_DRIVER",
  "APPLICATION",
  "0xA",
  "VOLUME"
};

// typedef Byte FFS_FILE_ATTRIBUTES;
// FFS File Attributes
#define FFS_ATTRIB_TAIL_PRESENT 0x01
// #define FFS_ATTRIB_RECOVERY 0x02
// #define FFS_ATTRIB_HEADER_EXTENSION 0x04
// #define FFS_ATTRIB_DATA_ALIGNMENT 0x38
#define FFS_ATTRIB_CHECKSUM 0x40

static const CUInt32PCharPair g_FFS_FILE_ATTRIBUTES[] =
{
  { 0, "" /* "TAIL" */ },
  { 1, "RECOVERY" },
  // { 2, "HEADER_EXTENSION" }, // reserved for future
  { 6, "" /* "CHECKSUM" */ }
};

// static const Byte g_Allignment[8] = { 3, 4, 7, 9, 10, 12, 15, 16 };

// typedef Byte FFS_FILE_STATE;

// Look also FVB_ERASE_POLARITY.
// Lower-order State bits are superceded by higher-order State bits.

// #define FILE_HEADER_CONSTRUCTION  0x01
// #define FILE_HEADER_VALID         0x02
#define FILE_DATA_VALID           0x04
// #define FILE_MARKED_FOR_UPDATE    0x08
// #define FILE_DELETED              0x10
// #define FILE_HEADER_INVALID       0x20

// SECTION_TYPE

#define SECTION_ALL 0x00

#define SECTION_COMPRESSION  0x01
#define SECTION_GUID_DEFINED 0x02

// Leaf section Type values
#define SECTION_PE32      0x10
#define SECTION_PIC       0x11
#define SECTION_TE        0x12
#define SECTION_DXE_DEPEX 0x13
#define SECTION_VERSION   0x14
#define SECTION_USER_INTERFACE 0x15
#define SECTION_COMPATIBILITY16 0x16
#define SECTION_FIRMWARE_VOLUME_IMAGE 0x17
#define SECTION_FREEFORM_SUBTYPE_GUID 0x18
#define SECTION_RAW       0x19
#define SECTION_PEI_DEPEX 0x1B


// #define GUIDED_SECTION_PROCESSING_REQUIRED 0x01
// #define GUIDED_SECTION_AUTH_STATUS_VALID 0x02

static const CUInt32PCharPair g_GUIDED_SECTION_ATTRIBUTES[] =
{
  { 0, "PROCESSING_REQUIRED" },
  { 1, "AUTH" }
};

static const CUInt32PCharPair g_SECTION_TYPE[] =
{
  { 0x01, "COMPRESSION" },
  { 0x02, "GUID" },
  { 0x10, "efi" },
  { 0x11, "PIC" },
  { 0x12, "te" },
  { 0x13, "DXE_DEPEX" },
  { 0x14, "VERSION" },
  { 0x15, "USER_INTERFACE" },
  { 0x16, "COMPATIBILITY16" },
  { 0x17, "VOLUME" },
  { 0x18, "FREEFORM_SUBTYPE_GUID" },
  { 0x19, "raw" },
  { 0x1B, "PEI_DEPEX" }
};

#define COMPRESSION_TYPE_NONE 0
#define COMPRESSION_TYPE_LZH  1
#define COMPRESSION_TYPE_LZMA 2

static const char *g_Methods[] =
{
  "COPY",
  "LZH",
  "LZMA"
};

static AString UInt32ToString(UInt32 val)
{
  char sz[16];
  ConvertUInt32ToString(val, sz);
  return sz;
}

static void ConvertByteToHex(unsigned value, char *s)
{
  for (int i = 0; i < 2; i++)
  {
    unsigned t = value & 0xF;
    value >>= 4;
    s[1 - i] = (char)((t < 10) ? ('0' + t) : ('A' + (t - 10)));
  }
}

static AString GuidToString(const Byte *p, bool full)
{
  char s[16 * 2 + 8];
  int i;
  for (i = 0; i < 4; i++)
    ConvertByteToHex(p[3 - i], s + i * 2);
  s[8] = 0;

  if (full)
  {
    s[8] = '-';
    for (i = 4; i < kGuidSize; i++)
      ConvertByteToHex(p[i], s + 1 + i * 2);
    s[32 + 1] = 0;
  }
  return s;
}

static const char *kExpressionCommands[] =
{
  "BEFORE", "AFTER", "PUSH", "AND", "OR", "NOT", "TRUE", "FALSE", "END", "SOR"
};

static bool ParseDepedencyExpression(const Byte *p, UInt32 size, AString &res)
{
  res.Empty();
  for (UInt32 i = 0; i < size;)
  {
    unsigned command = p[i++];
    if (command > ARRAY_SIZE(kExpressionCommands))
      return false;
    res += kExpressionCommands[command];
    if (command < 3)
    {
      if (i + kGuidSize > size)
        return false;
      res += " ";
      res += GuidToString(p + i, false);
      i += kGuidSize;
    }
    res += "; ";
  }
  return true;
}

static bool ParseUtf16zString(const Byte *p, UInt32 size, UString &res)
{
  if ((size & 1) != 0)
    return false;
  res.Empty();
  UInt32 i;
  for (i = 0; i < size; i += 2)
  {
    wchar_t c = Get16(p + i);
    if (c == 0)
      break;
    res += c;
  }
  return (i == size - 2);
}

static bool ParseUtf16zString2(const Byte *p, UInt32 size, AString &res)
{
  UString s;
  if (!ParseUtf16zString(p, size, s))
    return false;
  res = UnicodeStringToMultiByte(s);
  return true;
}

#define FLAGS_TO_STRING(pairs, value) FlagsToString(pairs, ARRAY_SIZE(pairs), value)
#define TYPE_TO_STRING(table, value) TypeToString(table, ARRAY_SIZE(table), value)
#define TYPE_PAIR_TO_STRING(table, value) TypePairToString(table, ARRAY_SIZE(table), value)

static const UInt32 kFileHeaderSize = 24;

static void AddSpaceAndString(AString &res, const AString &newString)
{
  if (!res.IsEmpty() && !newString.IsEmpty())
    res += ' ';
  res += newString;
}

class CFfsFileHeader
{
  Byte CheckHeader;
  Byte CheckFile;
  Byte Attrib;
  Byte State;

  UInt16 GetTailReference() const { return CheckHeader | ((UInt16)CheckFile << 8); }
  UInt32 GetTailSize() const { return IsThereTail() ? 2 : 0; }
  bool IsThereFileChecksum() const { return (Attrib & FFS_ATTRIB_CHECKSUM) != 0; }
  bool IsThereTail() const { return (Attrib & FFS_ATTRIB_TAIL_PRESENT) != 0; }
public:
  Byte GuidName[kGuidSize];
  Byte Type;
  UInt32 Size;
  
  bool Parse(const Byte *p)
  {
    int i;
    for (i = 0; i < kFileHeaderSize; i++)
      if (p[i] != 0xFF)
        break;
    if (i == kFileHeaderSize)
      return false;
    memcpy(GuidName, p, kGuidSize);
    CheckHeader = p[0x10];
    CheckFile = p[0x11];
    Type = p[0x12];
    Attrib = p[0x13];
    Size = Get24(p + 0x14);
    State = p[0x17];
    return true;
  }

  UInt32 GetDataSize() const { return Size - kFileHeaderSize - GetTailSize(); }

  bool Check(const Byte *p, UInt32 size)
  {
    if (Size > size)
      return false;
    UInt32 tailSize = GetTailSize();
    if (Size < kFileHeaderSize + tailSize)
      return false;
    
    {
      unsigned checkSum = 0;
      for (UInt32 i = 0; i < kFileHeaderSize; i++)
        checkSum += p[i];
      checkSum -= p[0x17];
      checkSum -= p[0x11];
      if ((Byte)checkSum != 0)
        return false;
    }
    
    if (IsThereFileChecksum())
    {
      unsigned checkSum = 0;
      UInt32 checkSize = Size - tailSize;
      for (UInt32 i = 0; i < checkSize; i++)
        checkSum += p[i];
      checkSum -= p[0x17];
      if ((Byte)checkSum != 0)
        return false;
    }
    
    if (IsThereTail())
      if (GetTailReference() != (UInt16)~Get16(p + Size - 2))
        return false;

    int polarity = 0;
    int i;
    for (i = 5; i >= 0; i--)
      if (((State >> i) & 1) == polarity)
      {
        // AddSpaceAndString(s, g_FFS_FILE_STATE_Flags[i]);
        if ((1 << i) != FILE_DATA_VALID)
          return false;
        break;
      }
    if (i < 0)
      return false;

    return true;
  }

  AString GetCharacts() const
  {
    AString s;
    if (Type == FV_FILETYPE_FFS_PAD)
      s += "PAD";
    else
      s += TYPE_TO_STRING(g_FileTypes, Type);
    AddSpaceAndString(s, FLAGS_TO_STRING(g_FFS_FILE_ATTRIBUTES, Attrib & 0xC7));
    /*
    int align = (Attrib >> 3) & 7;
    if (align != 0)
    {
      s += " Align:";
      s += UInt32ToString((UInt32)1 << g_Allignment[align]);
    }
    */
    return s;
  }
};

#define GET_32(offs, dest) dest = Get32(p + (offs));
#define GET_64(offs, dest) dest = Get64(p + (offs));

struct CCapsuleHeader
{
  UInt32 HeaderSize;
  UInt32 Flags;
  UInt32 CapsuleImageSize;
  UInt32 SequenceNumber;
  // Guid InstanceId;
  UInt32 OffsetToSplitInformation;
  UInt32 OffsetToCapsuleBody;
  UInt32 OffsetToOemDefinedHeader;
  UInt32 OffsetToAuthorInformation;
  UInt32 OffsetToRevisionInformation;
  UInt32 OffsetToShortDescription;
  UInt32 OffsetToLongDescription;
  UInt32 OffsetToApplicableDevices;

  void Clear() { memset(this, 0, sizeof(this)); }

  void Parse(const Byte *p)
  {
    GET_32(0x10, HeaderSize);
    GET_32(0x14, Flags);
    GET_32(0x18, CapsuleImageSize);
    GET_32(0x1C, SequenceNumber);
    GET_32(0x30, OffsetToSplitInformation);
    GET_32(0x34, OffsetToCapsuleBody);
    GET_32(0x38, OffsetToOemDefinedHeader);
    GET_32(0x3C, OffsetToAuthorInformation);
    GET_32(0x40, OffsetToRevisionInformation);
    GET_32(0x44, OffsetToShortDescription);
    GET_32(0x48, OffsetToLongDescription);
    GET_32(0x4C, OffsetToApplicableDevices);
  }
};

struct CItem
{
  AString Name;
  AString Characts;
  int Parent;
  int Method;
  int NameIndex;
  int NumChilds;
  bool IsDir;
  bool Skip;
  bool ThereAreSubDirs;
  bool ThereIsUniqueName;
  bool KeepName;

  int BufIndex;
  UInt32 Offset;
  UInt32 Size;

  CItem(): Parent(-1), Method(-1), NameIndex(-1), NumChilds(0),
      IsDir(false), Skip(false), ThereAreSubDirs(false), ThereIsUniqueName(false), KeepName(true) {}
  void SetGuid(const Byte *guidName, bool full = false);
  AString GetName(int numChildsInParent) const;
};

void CItem::SetGuid(const Byte *guidName, bool full)
{
  ThereIsUniqueName = true;
  int index = FindGuid(guidName);
  if (index >= 0)
    Name = kGuidNames[index];
  else
    Name = GuidToString(guidName, full);
}

AString CItem::GetName(int numChildsInParent) const
{
  if (numChildsInParent <= 1 || NameIndex < 0)
    return Name;
  char sz[32];
  char sz2[32];
  ConvertUInt32ToString(NameIndex, sz);
  ConvertUInt32ToString(numChildsInParent - 1, sz2);
  int numZeros = (int)strlen(sz2) - (int)strlen(sz);
  AString res;
  for (int i = 0; i < numZeros; i++)
    res += '0';
  return res + (AString)sz + '.' + Name;
}

struct CItem2
{
  AString Name;
  AString Characts;
  int MainIndex;
  int Parent;

  CItem2(): Parent(-1) {}
};

class CHandler:
  public IInArchive,
  public IInArchiveGetStream,
  public CMyUnknownImp
{
  CObjectVector<CItem> _items;
  CObjectVector<CItem2> _items2;
  CObjectVector<CByteBuffer> _bufs;
  UString _comment;
  UInt32 _methodsMask;
  bool _capsuleMode;

  UInt32 _totalBufsSize;
  CCapsuleHeader _h;

  void AddCommentString(const wchar_t *name, UInt32 pos);
  int AddItem(const CItem &item);
  int AddFileItemWithIndex(CItem &item);
  int AddDirItem(CItem &item);
  int AddBuf(UInt32 size);

  HRESULT ParseSections(int bufIndex, UInt32 pos, UInt32 size, int parent, int method, int level);
  HRESULT ParseVolume(int bufIndex, UInt32 posBase, UInt32 size, int parent, int method, int level);
  HRESULT OpenCapsule(IInStream *stream);
  HRESULT OpenFv(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback *callback);
  HRESULT Open2(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback *callback);
public:
  CHandler(bool capsuleMode): _capsuleMode(capsuleMode) {}
  MY_UNKNOWN_IMP2(IInArchive, IInArchiveGetStream)
  INTERFACE_IInArchive(;)
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);
};

static const STATPROPSTG kProps[] =
{
  { NULL, kpidPath, VT_BSTR},
  { NULL, kpidIsDir, VT_BOOL},
  { NULL, kpidSize, VT_UI8},
  { NULL, kpidMethod, VT_BSTR},
  { NULL, kpidCharacts, VT_BSTR}
};

static const STATPROPSTG kArcProps[] =
{
  { NULL, kpidComment, VT_BSTR},
  { NULL, kpidMethod, VT_BSTR},
  { NULL, kpidPhySize, VT_UI8},
  { NULL, kpidCharacts, VT_BSTR}
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  const CItem2 &item2 = _items2[index];
  const CItem &item = _items[item2.MainIndex];
  switch(propID)
  {
    case kpidPath:
    {
      AString path = item2.Name;
      int cur = item2.Parent;
      while (cur >= 0)
      {
        const CItem2 &item2 = _items2[cur];
        path = item2.Name + CHAR_PATH_SEPARATOR + path;
        cur = item2.Parent;
      }
      prop = path;
      break;
    }
    case kpidIsDir: prop = item.IsDir; break;
    case kpidMethod: if (item.Method >= 0) prop = g_Methods[item.Method]; break;
    case kpidCharacts: if (!item2.Characts.IsEmpty()) prop = item2.Characts; break;
    case kpidSize: if (!item.IsDir) prop = (UInt64)item.Size; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

void CHandler::AddCommentString(const wchar_t *name, UInt32 pos)
{
  UString s;
  const Byte *buf = _bufs[0];
  if (pos < _h.HeaderSize)
    return;
  for (UInt32 i = pos;; i += 2)
  {
    if (s.Length() > (1 << 16) || i >= _h.OffsetToCapsuleBody)
      return;
    wchar_t c = Get16(buf + i);
    if (c == 0)
    {
      i += 2;
      if (i >= _h.OffsetToCapsuleBody)
        return;
      c = Get16(buf + i);
      if (c == 0)
        break;
      s += L'\n';
    }
    s += c;
  }
  if (s.IsEmpty())
    return;
  _comment += L'\n';
  _comment += name;
  _comment += L": ";
  _comment += s;
}

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch(propID)
  {
    case kpidMethod:
    {
      AString s;
      for (int i = 0; i < 32; i++)
        if ((_methodsMask & ((UInt32)1 << i)) != 0)
          AddSpaceAndString(s, g_Methods[i]);
      if (!s.IsEmpty())
        prop = s;
      break;
    }
    case kpidComment: if (!_comment.IsEmpty()) prop = _comment; break;
    case kpidPhySize: prop = (UInt64)_h.CapsuleImageSize; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

#ifdef SHOW_DEBUG_INFO
static void PrintLevel(int level)
{
  PRF(printf("\n"));
  for (int i = 0; i < level; i++)
    PRF(printf("  "));
}
static void MyPrint(UInt32 posBase, UInt32 size, int level, const char *name)
{
  PrintLevel(level);
  PRF(printf("%s, pos = %6x, size = %6d", name, posBase, size));
}
#else
#define PrintLevel(level)
#define MyPrint(posBase, size, level, name)
#endif

static const unsigned kNumBigValueBits = 8 * 4;
static const unsigned kNumValueBytes = 3;
static const unsigned kNumValueBits = 8  * kNumValueBytes;
static const UInt32 kMask = (1 << kNumValueBits) - 1;

class CBitmMemDecoder
{
  unsigned _bitPos;
  UInt32 _value;
  const Byte *_buf;
  size_t _pos;
  size_t _size;
  size_t _extra;
public:
  void Init(const Byte *buf, size_t size)
  {
    _buf = buf;
    _size = size;
    _pos = 0;
    _extra = 0;
    _bitPos = kNumBigValueBits;
    Normalize();
  }
  
  bool IsFullFinished() const { return (_extra * 8) == (kNumBigValueBits - _bitPos); }
  
  void Normalize()
  {
    for (; _bitPos >= 8; _bitPos -= 8)
    {
      Byte b;
      if (_pos < _size)
        b = _buf[_pos++];
      else
      {
        b = 0;
        _extra++;
      }
      _value = (_value << 8) | b;
    }
  }

  UInt32 GetValue(unsigned numBits) const
  {
    return ((_value >> (8 - _bitPos)) & kMask) >> (kNumValueBits - numBits);
  }
  
  void MovePos(unsigned numBits)
  {
    _bitPos += numBits;
    Normalize();
  }
  
  UInt32 ReadBitsFast(unsigned numBits)
  {
    UInt32 res = GetValue(numBits);
    MovePos(numBits);
    return res;
  }

  UInt32 ReadBits(unsigned numBits);
  UInt32 ReadAlignBits() { return ReadBits((32 - _bitPos) & 7); }
};

UInt32 CBitmMemDecoder::ReadBits(unsigned numBits)
{
  UInt32 res = GetValue(numBits);
  MovePos(numBits);
  return res;
}

namespace NHuffman {

static const int kNumTableBits = 9;
static const int kNumBitsMax = 16;

class CDecoder
{
  UInt32 m_Limits[kNumBitsMax + 1];
  UInt32 m_Positions[kNumBitsMax + 1];
  Byte m_Lengths[1 << kNumTableBits];
  Int32 m_MainSymbol;

public:
  UInt32 *m_Symbols;
  UInt32 m_NumSymbols;

  void SetSingleSymbolMode(UInt32 symbol) { m_MainSymbol = symbol; }
  bool SetCodeLengths(const Byte *codeLengths);
  UInt32 DecodeSymbol(CBitmMemDecoder *bitStream)
  {
    if (m_MainSymbol != -1)
      return (UInt32)m_MainSymbol;
    int numBits;
    UInt32 value = bitStream->GetValue(kNumBitsMax);
    if (value < m_Limits[kNumTableBits])
      numBits = m_Lengths[value >> (kNumBitsMax - kNumTableBits)];
    else
      for (numBits = kNumTableBits + 1; value >= m_Limits[numBits]; numBits++);
    bitStream->MovePos(numBits);
    return m_Symbols[m_Positions[numBits] + ((value - m_Limits[numBits - 1]) >> (kNumBitsMax - numBits))];
  }
};

bool CDecoder::SetCodeLengths(const Byte *codeLengths)
{
  m_MainSymbol = -1;
  int lenCounts[kNumBitsMax + 1];
  UInt32 tmpPositions[kNumBitsMax + 1];
  int i;
  for (i = 1; i <= kNumBitsMax; i++)
    lenCounts[i] = 0;
  UInt32 symbol;
  for (symbol = 0; symbol < m_NumSymbols; symbol++)
  {
    int len = codeLengths[symbol];
    if (len > kNumBitsMax)
      return false;
    lenCounts[len]++;
    m_Symbols[symbol] = 0xFFFFFFFF;
  }
  lenCounts[0] = 0;
  m_Positions[0] = m_Limits[0] = 0;
  UInt32 startPos = 0;
  UInt32 index = 0;
  const UInt32 kMaxValue = (1 << kNumBitsMax);
  for (i = 1; i <= kNumBitsMax; i++)
  {
    startPos += lenCounts[i] << (kNumBitsMax - i);
    if (startPos > kMaxValue)
      return false;
    m_Limits[i] = (i == kNumBitsMax) ? kMaxValue : startPos;
    m_Positions[i] = m_Positions[i - 1] + lenCounts[i - 1];
    tmpPositions[i] = m_Positions[i];
    if (i <= kNumTableBits)
    {
      UInt32 limit = (m_Limits[i] >> (kNumBitsMax - kNumTableBits));
      for (; index < limit; index++)
        m_Lengths[index] = (Byte)i;
    }
  }
  if (startPos != kMaxValue)
    return false;
  for (symbol = 0; symbol < m_NumSymbols; symbol++)
  {
    int len = codeLengths[symbol];
    if (len != 0)
      m_Symbols[tmpPositions[len]++] = symbol;
  }
  return true;
}

}

static const int kMaxHuffmanLen = 16;
static const int kExtraSize = kMaxHuffmanLen + 3;
static const int kMinMatchLen = 3;
static const int kMaxMatchLen = 256;
static const int kNumAlphaSymsMax = 256 + kMaxMatchLen - kMinMatchLen + 1;
static const int kNumDistSymsMax = 24 + 2; // it's limited by bit decoder.

#define HUFF_START_CODE(huff, numSymsMax, numSymBits) \
  UInt32 numSyms = bitDec.ReadBits(numSymBits); \
  Byte lens[numSymsMax]; memset(lens, 0, sizeof(lens)); \
  if (numSyms > (numSymsMax)) return S_FALSE; \
  huff.m_NumSymbols = numSyms; \
  if (numSyms == 0) { \
    numSyms = bitDec.ReadBits(numSymBits); \
    if (numSyms >= (numSymsMax)) return S_FALSE; \
    huff.SetSingleSymbolMode(numSyms); } \

static HRESULT LzhDecode(Byte *dest, UInt32 destSize, const Byte *src, UInt32 srcSize)
{
  if (srcSize < 8)
    return S_FALSE;
  {
    UInt32 packSize = Get32(src);
    UInt32 unpackSize = Get32(src + 4);
    src += 8;
    srcSize -= 8;
    if (destSize != unpackSize || srcSize != packSize)
      return S_FALSE;
  }

  CBitmMemDecoder bitDec;
  bitDec.Init(src, srcSize);

  UInt32 pos = 0;
  for (;;)
  {
    UInt32 blockSize = bitDec.ReadBits(16);
    UInt32 symbols[kExtraSize + kNumAlphaSymsMax + kNumDistSymsMax];

    NHuffman::CDecoder extraHuff;
    extraHuff.m_Symbols = symbols;
    {
      HUFF_START_CODE(extraHuff, kExtraSize, 5)
      else
      {
        for (UInt32 i = 0; i < numSyms; i++)
        {
          if (i == 3)
          {
            UInt32 numZeros = bitDec.ReadBits(2);
            if (i + numZeros > numSyms)
              return S_FALSE;
            for (UInt32 j = 0; j < numZeros; j++, i++)
              lens[i] = (Byte)0;
            if (i == numSyms)
              break;
          }
          
          UInt32 len = bitDec.ReadBits(3);
          if (len == 7)
          {
            for(;; len++)
            {
              if (len > kMaxHuffmanLen)
                return S_FALSE;
              if (bitDec.ReadBits(1) == 0)
                break;
            }
          }
          lens[i] = (Byte)len;
        }
        if (!extraHuff.SetCodeLengths(lens))
          return S_FALSE;
      }
    }

    NHuffman::CDecoder symHuff;
    symHuff.m_Symbols = symbols + kExtraSize;
    {
      HUFF_START_CODE(symHuff, kNumAlphaSymsMax, 9)
      else
      {
        for (UInt32 i = 0; i < numSyms;)
        {
          UInt32 c = extraHuff.DecodeSymbol(&bitDec);
          if (c > 2)
            lens[i++] = (Byte)c - 2;
          else
          {
            UInt32 numZeros;
            if (c == 0)
              numZeros = 1;
            else if (c == 1)
              numZeros = bitDec.ReadBits(4) + 3;
            else
              numZeros = bitDec.ReadBits(9) + 20;
            if (i + numZeros > numSyms)
              return S_FALSE;
            for (UInt32 j = 0; j < numZeros; j++, i++)
              lens[i] = (Byte)0;
          }
        }
        if (!symHuff.SetCodeLengths(lens))
          return S_FALSE;
      }
    }

    NHuffman::CDecoder distHuff;
    distHuff.m_Symbols = symbols + kExtraSize + kNumAlphaSymsMax;
    {
      const UInt32 version = 1;
      const UInt32 numDistBits = version + 4;
      HUFF_START_CODE(distHuff, kNumDistSymsMax, numDistBits)
      else
      {
        for (UInt32 i = 0; i < numSyms; i++)
        {
          UInt32 len = bitDec.ReadBits(3);
          if (len == 7)
          {
            for(;; len++)
            {
              if (len > kMaxHuffmanLen)
                return S_FALSE;
              if (bitDec.ReadBits(1) == 0)
                break;
            }
          }
          lens[i] = (Byte)len;
        }
        if (!distHuff.SetCodeLengths(lens))
          return S_FALSE;
      }
    }

    while (blockSize)
    {
      blockSize--;
      UInt32 c = symHuff.DecodeSymbol(&bitDec);
      if (c < 256)
      {
        if (destSize == 0)
          return S_FALSE;
        *dest++ = (Byte)c;
        destSize--;
        pos++;
        continue;
      }
      c = c - 256 + kMinMatchLen;
      if (destSize < c)
        return S_FALSE;
      UInt32 dist = distHuff.DecodeSymbol(&bitDec);
      if (dist > 1)
        dist = ((UInt32)1 << (dist - 1)) + bitDec.ReadBits(dist - 1);
      dist++;
      if (dist > pos)
        return S_FALSE;
      pos += c;
      destSize -= c;
      do
      {
        *dest = dest[0 - (Int32)dist];
        dest++;
      }
      while (--c);
    }

    // PRF(printf("\ndestSize = %6d", destSize));
    if (destSize == 0)
    {
      if (bitDec.ReadAlignBits() != 0)
        return S_FALSE;
      if (bitDec.ReadBits(8) != 0)
        return S_FALSE;
     if (!bitDec.IsFullFinished())
        return S_FALSE;
      break;
    }
  }
  return S_OK;
}

int CHandler::AddItem(const CItem &item)
{
  if (_items.Size() >= kNumFilesMax)
    throw 2;
  return _items.Add(item);
}

int CHandler::AddFileItemWithIndex(CItem &item)
{
  int nameIndex = _items.Size();
  if (item.Parent >= 0)
    nameIndex = _items[item.Parent].NumChilds++;
  item.NameIndex = nameIndex;
  return AddItem(item);
}

int CHandler::AddDirItem(CItem &item)
{
  if (item.Parent >= 0)
    _items[item.Parent].ThereAreSubDirs = true;
  item.IsDir = true;
  item.Size = 0;
  return AddItem(item);
}

int CHandler::AddBuf(UInt32 size)
{
  if (size > kBufTotalSizeMax - _totalBufsSize)
    throw 1;
  _totalBufsSize += size;
  int index = _bufs.Add(CByteBuffer());
  _bufs[index].SetCapacity(size);
  return index;
}

HRESULT CHandler::ParseSections(int bufIndex, UInt32 posBase, UInt32 size, int parent, int method, int level)
{
  if (level > kLevelMax)
    return S_FALSE;
  MyPrint(posBase, size, level, "Sections");
  level++;
  const Byte *bufData = _bufs[bufIndex];
  UInt32 pos = 0;
  for (;;)
  {
    if (size == pos)
      return S_OK;
    PrintLevel(level);
    PRF(printf("%s, pos = %6x", "Sect", pos));
    pos = (pos + 3) & ~(UInt32)3;
    if (pos > size)
      return S_FALSE;
    UInt32 rem = size - pos;
    if (rem == 0)
      return S_OK;
    if (rem < 4)
      return S_FALSE;
    const Byte *p = bufData + posBase + pos;
    UInt32 sectSize = Get24(p);
    if (sectSize > rem || sectSize < 4)
      return S_FALSE;
    
    Byte type = p[3];
    PrintLevel(level);
    PRF(printf("%s, type = %2x, pos = %6x, size = %6d", "Sect", type, pos, sectSize));
    CItem item;
    item.Method = method;
    item.BufIndex = bufIndex;
    item.Parent = parent;
    item.Offset = posBase + pos + 4;
    UInt32 sectDataSize = sectSize - 4;
    item.Size = sectDataSize;
    item.Name = TYPE_PAIR_TO_STRING(g_SECTION_TYPE, type);

    if (type == SECTION_COMPRESSION)
    {
      if (sectSize < 4 + 5)
        return S_FALSE;
      UInt32 uncompressedSize = Get32(p + 4);
      Byte compressionType = p[8];

      UInt32 newSectSize = sectSize - 9;
      UInt32 newOffset = posBase + pos + 9;
      const Byte *pStart = p + 9;

      item.KeepName = false;
      if (compressionType > 2)
      {
        // AddFileItemWithIndex(item);
        return S_FALSE;
      }
      else
      {
        item.Name = g_Methods[compressionType];
        // int parent = AddDirItem(item);
        if (compressionType == COMPRESSION_TYPE_NONE)
        {
          RINOK(ParseSections(bufIndex, newOffset, newSectSize, parent, method, level));
        }
        else if (compressionType == COMPRESSION_TYPE_LZH)
        {
          int newBufIndex = AddBuf(uncompressedSize);
          CByteBuffer &buf = _bufs[newBufIndex];
          RINOK(LzhDecode(buf, uncompressedSize, pStart, newSectSize));
          RINOK(ParseSections(newBufIndex, 0, uncompressedSize, parent, compressionType, level));
        }
        else
        {
          if (newSectSize < 4 + 5 + 8)
            return S_FALSE;
          unsigned addSize = 4;
          if (pStart[0] == 0x5d && pStart[1] == 0 && pStart[2] == 0 && pStart[3] == 0x80 && pStart[4] == 0)
          {
            addSize = 0;
            // some archives have such header
          }
          else
          {
            // normal BIOS contains uncompressed size here
            // UInt32 uncompressedSize2 = Get24(pStart);
            // Byte firstSectType = p[9 + 3];
            // firstSectType can be 0 in some archives
          }
          pStart += addSize;
          UInt64 lzmaUncompressedSize = Get64(pStart + 5);
          if (lzmaUncompressedSize > (1 << 30))
            return S_FALSE;
          if (lzmaUncompressedSize < uncompressedSize)
            return S_FALSE;
          SizeT destLen = (SizeT)lzmaUncompressedSize;
          int newBufIndex = AddBuf((UInt32)lzmaUncompressedSize);
          CByteBuffer &buf = _bufs[newBufIndex];
          ELzmaStatus status;
          SizeT srcLen = newSectSize - (addSize + 5 + 8);
          SizeT srcLen2 = srcLen;
          SRes res = LzmaDecode(buf, &destLen, pStart + 13, &srcLen,
              pStart, 5, LZMA_FINISH_END, &status, &g_Alloc);
          if (res != 0)
            return S_FALSE;
          if (srcLen != srcLen2 || destLen != lzmaUncompressedSize || status != LZMA_STATUS_FINISHED_WITH_MARK)
            return S_FALSE;
          RINOK(ParseSections(newBufIndex, 0, (UInt32)lzmaUncompressedSize, parent, compressionType, level));
        }
        _methodsMask |= (1 << compressionType);
      }
    }
    else if (type == SECTION_GUID_DEFINED)
    {
      const UInt32 kHeaderSize = 4 + kGuidSize + 4;
      if (sectSize < kHeaderSize)
        return S_FALSE;
      item.SetGuid(p + 4);
      UInt32 dataOffset = Get16(p + 4 + kGuidSize);
      UInt32 attrib = Get16(p + 4 + kGuidSize + 2);
      if (dataOffset > sectSize || dataOffset < kHeaderSize)
        return S_FALSE;
      UInt32 newSectSize = sectSize - dataOffset;
      item.Size = newSectSize;
      UInt32 newOffset = posBase + pos + dataOffset;
      item.Offset = newOffset;
      UInt32 propsSize = dataOffset - kHeaderSize;
      bool needDir = true;
      AddSpaceAndString(item.Characts, FLAGS_TO_STRING(g_GUIDED_SECTION_ATTRIBUTES, attrib));
      if (AreGuidsEq(p + 0x4, kGuids[kGuidIndex_CRC]) && propsSize == 4)
      {
        needDir = false;
        item.KeepName = false;
        if (CrcCalc(bufData + newOffset, newSectSize) != Get32(p + kHeaderSize))
          return S_FALSE;
      }
      else
      {
        if (propsSize != 0)
        {
          CItem item2 = item;
          item2.Name += ".prop";
          item2.Size = propsSize;
          item2.Offset = posBase + pos + kHeaderSize;
          AddItem(item2);
        }
      }
      int newParent = parent;
      if (needDir)
        newParent = AddDirItem(item);
      RINOK(ParseSections(bufIndex, newOffset, newSectSize, newParent, method, level));
    }
    else if (type == SECTION_FIRMWARE_VOLUME_IMAGE)
    {
      item.KeepName = false;
      int newParent = AddDirItem(item);
      RINOK(ParseVolume(bufIndex, posBase + pos + 4, sectSize - 4, newParent, method, level));
    }
    else
    {
      bool needAdd = true;
      switch(type)
      {
        case SECTION_RAW:
        {
          const UInt32 kInsydeOffset = 12;
          if (sectDataSize >= kFvHeaderSize + kInsydeOffset)
          {
            if (IsFfs(p + 4 + kInsydeOffset) &&
                sectDataSize - kInsydeOffset == Get64(p + 4 + kInsydeOffset + 0x20))
            {
              needAdd = false;
              item.Name = "vol";
              int newParent = AddDirItem(item);
              RINOK(ParseVolume(bufIndex, posBase + pos + 4 + kInsydeOffset, sectDataSize - kInsydeOffset, newParent, method, level));
            }
            
            if (needAdd)
            {
              const char *ext = FindExt(p + 4, sectDataSize);
              if (ext)
                item.Name = ext;
            }
          }
          break;
        }
        case SECTION_DXE_DEPEX:
        case SECTION_PEI_DEPEX:
        {
          AString s;
          if (ParseDepedencyExpression(p + 4, sectDataSize, s))
          {
            if (s.Length() < (1 << 9))
            {
              s = '[' + s + ']';
              AddSpaceAndString(_items[item.Parent].Characts, s);
              needAdd = false;
            }
            else
            {
              item.BufIndex = AddBuf(s.Length());
              CByteBuffer &buf0 = _bufs[item.BufIndex];
              memcpy(buf0, s, s.Length());
              item.Offset = 0;
              item.Size = s.Length();
            }
          }
          break;
        }
        case SECTION_VERSION:
        {
          if (sectDataSize > 2)
          {
            AString s;
            if (ParseUtf16zString2(p + 6, sectDataSize - 2, s))
            {
              AddSpaceAndString(_items[item.Parent].Characts, (AString)"ver:" + UInt32ToString(Get16(p + 4)) + ' ' + s);
              needAdd = false;
            }
          }
          break;
        }
        case SECTION_USER_INTERFACE:
        {
          AString s;
          if (ParseUtf16zString2(p + 4, sectDataSize, s))
          {
            _items[parent].Name = s;
            needAdd = false;
          }
          break;
        }
        case SECTION_FREEFORM_SUBTYPE_GUID:
        {
          if (sectDataSize >= kGuidSize)
          {
            item.SetGuid(p + 4);
            item.Size = sectDataSize - kGuidSize;
            item.Offset = posBase + pos + 4 + kGuidSize;
          }
          break;
        }
      }
    
      if (needAdd)
        AddFileItemWithIndex(item);
    }
    pos += sectSize;
  }
}

static UInt32 Count_FF_Bytes(const Byte *p, UInt32 size)
{
  UInt32 i;
  for (i = 0; i < size && p[i] == 0xFF; i++);
  return i;
}

static bool Is_FF_Stream(const Byte *p, UInt32 size)
{
  return (Count_FF_Bytes(p, size) == size);
}

HRESULT CHandler::ParseVolume(int bufIndex, UInt32 posBase, UInt32 size, int parent, int method, int level)
{
  if (level > kLevelMax)
    return S_FALSE;
  MyPrint(posBase, size, level, "Volume");
  level++;
  if (size < kFvHeaderSize)
    return S_FALSE;
  const Byte *p = _bufs[bufIndex] + posBase;
  // first 16 bytes must be zeros, but they are not zeros sometimes.
  if (!AreGuidsEq(p + kFfsGuidOffset, k_FFS_Guid) &&
      !AreGuidsEq(p + kFfsGuidOffset, k_MacFS_Guid))
  {
    CItem item;
    item.Method = method;
    item.BufIndex = bufIndex;
    item.Parent = parent;
    item.Offset = posBase;
    item.Size = size;
    item.SetGuid(p + kFfsGuidOffset);
    item.Name += " [VOLUME]";
    AddItem(item);
    return S_OK;
  }
  
  if (Get32(p + 0x28) != kFvSignature)
    return S_FALSE;
  UInt32 attribs = Get32(p + 0x2C);
  if ((attribs & FVB_ERASE_POLARITY) == 0)
    return S_FALSE;
  // if (parent >= 0) AddSpaceAndString(_items[parent].Characts, FLAGS_TO_STRING(g_FV_Attribs, attribs));
  UInt64 fvLen = Get64(p + 0x20);
  UInt32 headerLen = Get16(p + 0x30);
  if (headerLen > size || headerLen < kFvHeaderSize || (headerLen & 0x7) != 0 ||
      fvLen > size || fvLen < headerLen)
    return S_FALSE;
  
  {
    UInt32 checkCalc = 0;
    for (UInt32 i = 0; i < headerLen; i += 2)
      checkCalc += Get16(p + i);
    if ((checkCalc & 0xFFFF) != 0)
      return S_FALSE;
  }
  
  // 3 reserved bytes are not zeros sometimes.
  // UInt16 ExtHeaderOffset; // in new SPECIFICATION?
  // Byte revision = p[0x37];
  
  UInt32 pos = kFvHeaderSize;
  for (;;)
  {
    if (pos >= headerLen)
      return S_FALSE;
    UInt32 numBlocks = Get32(p + pos);
    UInt32 length = Get32(p + pos + 4);
    pos += 8;
    if (numBlocks == 0 && length == 0)
      break;
  }
  if (pos != headerLen)
    return S_FALSE;
  
  CRecordVector<UInt32> guidsVector;
  
  for (;;)
  {
    UInt32 rem = (UInt32)fvLen - pos;
    if (rem < kFileHeaderSize)
      break;
    pos = (pos + 7) & ~7;
    rem = (UInt32)fvLen - pos;
    if (rem < kFileHeaderSize)
      break;

    CItem item;
    item.Method = method;
    item.BufIndex = bufIndex;
    item.Parent = parent;

    const Byte *pFile = p + pos;
    CFfsFileHeader fh;
    if (!fh.Parse(pFile))
    {
      UInt32 num_FF_bytes = Count_FF_Bytes(pFile, rem);
      if (num_FF_bytes != rem)
      {
        item.Name = "[junk]";
        item.Offset = posBase + pos + num_FF_bytes;
        item.Size = rem - num_FF_bytes;
        AddItem(item);
      }
      break;
    }
    PrintLevel(level); PRF(printf("%s, pos = %6x, size = %6d", "FILE", posBase + pos, fh.Size));
    if (!fh.Check(pFile, rem))
      return S_FALSE;
    
    UInt32 offset = posBase + pos + kFileHeaderSize;
    UInt32 sectSize = fh.GetDataSize();
    item.Offset = offset;
    item.Size = sectSize;

    pos += fh.Size;

    if (fh.Type == FV_FILETYPE_FFS_PAD)
      if (Is_FF_Stream(pFile + kFileHeaderSize, sectSize))
        continue;
    
    UInt32 guid32 = Get32(fh.GuidName);
    bool full = true;
    if (guidsVector.FindInSorted(guid32) < 0)
    {
      guidsVector.AddToUniqueSorted(guid32);
      full = false;
    }
    item.SetGuid(fh.GuidName, full);
    
    item.Characts = fh.GetCharacts();
    PrintLevel(level);
    PRF(printf("%s", item.Characts));
    
    if (fh.Type == FV_FILETYPE_FFS_PAD ||
        fh.Type == FV_FILETYPE_RAW)
    {
      bool isVolume = false;
      if (fh.Type == FV_FILETYPE_RAW)
      {
        if (sectSize >= kFvHeaderSize)
          if (IsFfs(pFile + kFileHeaderSize))
            isVolume = true;
      }
      if (isVolume)
      {
        int newParent = AddDirItem(item);
        RINOK(ParseVolume(bufIndex, offset, sectSize, newParent, method, level));
      }
      else
        AddItem(item);
    }
    else
    {
      int newParent = AddDirItem(item);
      RINOK(ParseSections(bufIndex, offset, sectSize, newParent, method, level));
    }
  }
  return S_OK;
}

HRESULT CHandler::OpenCapsule(IInStream *stream)
{
  const UInt32 kHeaderSize = 80;
  Byte buf[kHeaderSize];
  RINOK(ReadStream_FALSE(stream, buf, kHeaderSize));
  _h.Parse(buf);
  if (_h.HeaderSize != kHeaderSize ||
      _h.CapsuleImageSize < kHeaderSize ||
      _h.OffsetToCapsuleBody < kHeaderSize ||
      _h.OffsetToCapsuleBody > _h.CapsuleImageSize)
    return S_FALSE;

  if (_h.SequenceNumber != 0 ||
      _h.OffsetToSplitInformation != 0 )
    return E_NOTIMPL;

  int bufIndex = AddBuf(_h.CapsuleImageSize);
  CByteBuffer &buf0 = _bufs[bufIndex];
  memcpy(buf0, buf, kHeaderSize);
  ReadStream_FALSE(stream, buf0 + kHeaderSize, _h.CapsuleImageSize - kHeaderSize);

  AddCommentString(L"Author", _h.OffsetToAuthorInformation);
  AddCommentString(L"Revision", _h.OffsetToRevisionInformation);
  AddCommentString(L"Short Description", _h.OffsetToShortDescription);
  AddCommentString(L"Long Description", _h.OffsetToLongDescription);

  return ParseVolume(bufIndex, _h.OffsetToCapsuleBody, _h.CapsuleImageSize - _h.OffsetToCapsuleBody, -1, -1, 0);
}

HRESULT CHandler::OpenFv(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback *callback)
{
  UInt64 fileSize;
  RINOK(stream->Seek(0, STREAM_SEEK_END, &fileSize));
  if (fileSize > (1 << 27))
    return S_FALSE;

  UInt32 volIndex = 0;
  UInt32 pos = 0, prevEnd = 0;
  if (callback)
  {
    RINOK(callback->SetTotal(NULL, &fileSize));
  }

  for (;;)
  {
    UInt64 limit = 0;
    UInt64 *limitPtr = NULL;
    if (maxCheckStartPosition)
    {
      UInt32 directSize = pos - prevEnd;
      if (directSize >= *maxCheckStartPosition)
        break;
      limit = *maxCheckStartPosition - directSize;
      limitPtr = &limit;
    }

    UInt64 resPos;
    RINOK(stream->Seek(pos + kFfsGuidOffset, STREAM_SEEK_SET, NULL));
    if (FindSignatureInStream(stream, k_FFS_Guid, kGuidSize, limitPtr, resPos) == S_FALSE)
      break;

    pos += (UInt32)resPos;
    UInt64 fvSize;
    {
      UInt32 rem = (UInt32)fileSize - pos;
      if (rem < kFvHeaderSize)
        break;
      RINOK(stream->Seek(pos, STREAM_SEEK_SET, NULL));
      Byte buf[kFvHeaderSize];
      RINOK(ReadStream_FALSE(stream, buf, kFvHeaderSize));
      fvSize = Get64(buf + 0x20);
      if (!IsFfs(buf) || fvSize > rem)
      {
        pos++;
        continue;
      }
    }

    RINOK(stream->Seek(prevEnd, STREAM_SEEK_SET, NULL));

    if (pos != prevEnd)
    {
      CItem item;
      item.Offset = 0;
      item.Size = pos - prevEnd;
      item.BufIndex = AddBuf(item.Size);
      CByteBuffer &buf0 = _bufs[item.BufIndex];
      RINOK(ReadStream_FALSE(stream, buf0, item.Size));
      item.Name = UInt32ToString(volIndex++);
      AddItem(item);
    }

    prevEnd = pos;
    RINOK(stream->Seek(pos, STREAM_SEEK_SET, NULL));
    UInt32 fvSize32 = (UInt32)fvSize;
    CItem item;
    item.BufIndex = AddBuf(fvSize32);
    CByteBuffer &buf0 = _bufs[item.BufIndex];
    item.Name = UInt32ToString(volIndex++);
    int parent = AddDirItem(item);
    ReadStream_FALSE(stream, buf0, fvSize32);
    RINOK(ParseVolume(item.BufIndex, 0, fvSize32, parent, -1, 0));
    pos += fvSize32;
    prevEnd = pos;

    if (callback)
    {
      UInt64 pos64 = pos;
      RINOK(callback->SetCompleted(NULL, &pos64));
    }
  }
  if (_items.Size() == 0)
    return S_FALSE;

  if (pos <= fileSize)
  {
    pos = (UInt32)fileSize;
    if (prevEnd < pos)
    {
      CItem item;
      item.Offset = 0;
      item.Size = pos - prevEnd;
      item.BufIndex = AddBuf(item.Size);
      CByteBuffer &buf0 = _bufs[item.BufIndex];
      RINOK(stream->Seek(prevEnd, STREAM_SEEK_SET, NULL));
      RINOK(ReadStream_FALSE(stream, buf0, item.Size));
      item.Name = UInt32ToString(volIndex++);
      AddItem(item);
    }
  }
  _h.CapsuleImageSize = pos;
  return S_OK;
}

HRESULT CHandler::Open2(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback *callback)
{
  if (_capsuleMode)
  {
    RINOK(OpenCapsule(stream));
  }
  else
  {
    RINOK(OpenFv(stream, maxCheckStartPosition, callback));
  }

  CIntVector numChilds;
  numChilds.Reserve(_items.Size());
  int i;
  for (i = 0; i < _items.Size(); i++)
  {
    numChilds.Add(0);
    int parent = _items[i].Parent;
    if (parent >= 0)
      numChilds[parent]++;
  }

  for (i = 0; i < _items.Size(); i++)
  {
    CItem &item = _items[i];
    int parent = item.Parent;
    if (parent >= 0)
    {
      CItem &parentItem = _items[parent];
      if (numChilds[parent] == 1)
        if (!item.ThereIsUniqueName || !parentItem.ThereIsUniqueName || !parentItem.ThereAreSubDirs)
          parentItem.Skip = true;
    }
  }

  CIntVector mainToReduced;
  for (i = 0; i < _items.Size(); i++)
  {
    mainToReduced.Add(_items2.Size());
    const CItem &item = _items[i];
    if (item.Skip)
      continue;
    AString name;
    int numItems = -1;
    int parent = item.Parent;
    if (parent >= 0)
      numItems = numChilds[parent];
    AString name2 = item.GetName(numItems);
    AString characts2 = item.Characts;
    if (item.KeepName)
      name = name2;
    while (parent >= 0)
    {
      const CItem &item3 = _items[parent];
      if (!item3.Skip)
        break;
      if (item3.KeepName)
      {
        AString name3 = item3.GetName(-1);
        if (name.IsEmpty())
          name = name3;
        else
          name = name3 + '.' + name;
      }
      AddSpaceAndString(characts2, item3.Characts);
      parent = item3.Parent;
    }
    if (name.IsEmpty())
      name = name2;
    
    CItem2 item2;
    item2.MainIndex = i;
    item2.Name = name;
    item2.Characts = characts2;
    if (parent >= 0)
      item2.Parent = mainToReduced[parent];
    _items2.Add(item2);
    /*
    CItem2 item2;
    item2.MainIndex = i;
    item2.Name = item.Name;
    item2.Parent = item.Parent;
    _items2.Add(item2);
    */
  }
  return S_OK;
}

STDMETHODIMP CHandler::Open(IInStream *inStream,
    const UInt64 *maxCheckStartPosition,
    IArchiveOpenCallback *callback)
{
  COM_TRY_BEGIN
  Close();
  try
  {
    if (Open2(inStream, maxCheckStartPosition, callback) != S_OK)
      return S_FALSE;
  }
  catch(...) { return S_FALSE; }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::Close()
{
  _totalBufsSize = 0;
  _methodsMask = 0;
  _items.Clear();
  _items2.Clear();
  _bufs.Clear();
  _comment.Empty();
  _h.Clear();
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _items2.Size();
  return S_OK;
}

STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  bool allFilesMode = (numItems == (UInt32)-1);
  if (allFilesMode)
    numItems = _items2.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
    totalSize += _items[_items2[allFilesMode ? i : indices[i]].MainIndex].Size;
  extractCallback->SetTotal(totalSize);

  UInt64 currentTotalSize = 0;
  
  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  for (i = 0; i < numItems; i++)
  {
    lps->InSize = lps->OutSize = currentTotalSize;
    RINOK(lps->SetCur());
    CMyComPtr<ISequentialOutStream> realOutStream;
    Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    UInt32 index = allFilesMode ? i : indices[i];
    const CItem &item = _items[_items2[index].MainIndex];
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode));
    currentTotalSize += item.Size;
    
    if (!testMode && !realOutStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode));
    if (testMode || item.IsDir)
    {
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK));
      continue;
    }
    int res = NExtract::NOperationResult::kDataError;
    CMyComPtr<ISequentialInStream> inStream;
    GetStream(index, &inStream);
    if (inStream)
    {
      RINOK(copyCoder->Code(inStream, realOutStream, NULL, NULL, progress));
      if (copyCoderSpec->TotalSize == item.Size)
        res = NExtract::NOperationResult::kOK;
    }
    realOutStream.Release();
    RINOK(extractCallback->SetOperationResult(res));
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::GetStream(UInt32 index, ISequentialInStream **stream)
{
  COM_TRY_BEGIN
  const CItem &item = _items[_items2[index].MainIndex];
  if (item.IsDir)
    return S_FALSE;
  CBufInStream *streamSpec = new CBufInStream;
  CMyComPtr<IInStream> streamTemp = streamSpec;
  const CByteBuffer &buf = _bufs[item.BufIndex];
  /*
  if (item.Offset + item.Size > buf.GetCapacity())
    return S_FALSE;
  */
  streamSpec->Init(buf + item.Offset, item.Size, (IInArchive *)this);
  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}


namespace UEFIc
{
  static IInArchive *CreateArc() { return new CHandler(true); }
  static CArcInfo g_ArcInfo =
    { L"UEFIc", L"scap", 0, 0xD0, CAPSULE_SIGNATURE, kCapsuleSigSize, false, CreateArc, 0 };
  REGISTER_ARC(UEFIc)
}

namespace UEFIs
{
  static IInArchive *CreateArc() { return new CHandler(false); }
  static CArcInfo g_ArcInfo =
    { L"UEFIs", L"", 0, 0xD1, FFS_SIGNATURE, kGuidSize, false, CreateArc, 0 };
  REGISTER_ARC(UEFIs)
}

}}
