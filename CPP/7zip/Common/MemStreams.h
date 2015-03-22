// MemStreams.h

#ifndef __MEM_STREAMS_H
#define __MEM_STREAMS_H

#include <stdio.h>

#include "../../../C/Alloc.h"
#include "../../../C/7zCrc.h"

#include "../../Common/MyCom.h"


class CSeqInMemStream : public ISequentialInStream,
                        public CMyUnknownImp
{
  UInt32 _StreamSize;
  const Byte *_StreamData;
  UInt32 _StreamPos;
  UInt32 _crc;

public:
  CSeqInMemStream(const Byte* data, UInt32 size)
  : _StreamSize(size),
    _StreamData(data),
    _StreamPos(0),
    _crc(CRC_INIT_VAL)
  {}

  UInt32 GetCRC() const { return CRC_GET_DIGEST(_crc); }

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize)
  {
    UInt32 remain = _StreamSize - _StreamPos;
    if(size > remain) {
      size = remain;
    }
    memcpy(data, _StreamData+_StreamPos, size);
    _StreamPos += size;
    _crc = CrcUpdate(_crc, data, size);
   *processedSize = size;
    return S_OK;
  }

  MY_UNKNOWN_IMP
};

class CSeqOutMemStream : public ISequentialOutStream,
                         public CMyUnknownImp
{
  UInt32 _StreamSize;
  Byte *_StreamData;
  UInt32 _StreamPos;
  UInt32 _crc;

public:
  CSeqOutMemStream(Byte* data, UInt32 size)
  : _StreamSize(size),
    _StreamData(data),
    _StreamPos(0),
    _crc(CRC_INIT_VAL)
  {}

  UInt32 GetCRC() const { return CRC_GET_DIGEST(_crc); }
  UInt32 Tell() const { return _StreamPos; }

  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize)
  {
    UInt32 remain = _StreamSize - _StreamPos;
    if(size > remain) {
      size = remain;
    }
    memcpy(_StreamData+_StreamPos, data, size);
    _StreamPos += size;
    _crc = CrcUpdate(_crc, data, size);
   *processedSize = size;
    return S_OK;
  }

  MY_UNKNOWN_IMP
};

#endif
