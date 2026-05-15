// ZpaqHandler.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/MyString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/PropVariant.h"
#include "../../../Windows/TimeUtils.h"

#include "../../Common/ProgressUtils.h"
#include "../../Common/RegisterArc.h"
#include "../../Common/StreamUtils.h"

#include "ZpaqIn.h"

using namespace NWindows;

namespace NArchive {
namespace NZpaq {

Z7_CLASS_IMP_CHandler_IInArchive_0
  CMyComPtr<IInStream> _stream;
  CIndex _idx;
};

static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  kpidAttrib,
  kpidComment
};

static const Byte kArcProps[] =
{
  kpidNumImages,    // re-used to expose ZPAQ "number of versions"
  kpidPhySize
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

// Build the synthetic top-level path "vNNNN_YYYY-MM-DD_hh-mm-ss/<original path>".
// Version 0 is the streaming pseudo-version (no journaling); render it without
// timestamp so it round-trips visibly as just "v0000/".
static void BuildVersionedPath(const CVersion &ver, UInt32 verIndex,
    const AString &inner, AString &outPath)
{
  char buf[64];
  // ConvertUInt32ToString writes plain decimal; pad to 4 digits manually.
  char numBuf[16];
  ConvertUInt32ToString(verIndex, numBuf);
  size_t nlen = strlen(numBuf);
  size_t pos = 0;
  buf[pos++] = 'v';
  for (size_t i = nlen; i < 4; i++) buf[pos++] = '0';
  memcpy(buf + pos, numBuf, nlen); pos += nlen;

  if (ver.date != 0)
  {
    Int64 d = ver.date;
    int ss = (int)(d % 100); d /= 100;
    int mm = (int)(d % 100); d /= 100;
    int hh = (int)(d % 100); d /= 100;
    int dd = (int)(d % 100); d /= 100;
    int mo = (int)(d % 100); d /= 100;
    int yr = (int)d;
    pos += (size_t)sprintf(buf + pos, "_%04d-%02d-%02d_%02d-%02d-%02d",
        yr, mo, dd, hh, mm, ss);
  }
  buf[pos] = 0;

  outPath = buf;
  outPath += '/';
  outPath += inner;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _idx.files.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidNumImages: prop = (UInt32)_idx.versions.Size(); break;
    case kpidPhySize:   prop = (UInt64)_idx.archiveSize; break;
  }
  prop.Detach(value);
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  if (index >= (UInt32)_idx.files.Size())
    return E_INVALIDARG;

  const CFile &f = _idx.files[index];
  const CVersion &v = _idx.versions[f.versionIndex];

  switch (propID)
  {
    case kpidPath:
    {
      AString p;
      BuildVersionedPath(v, f.versionIndex, f.path, p);
      prop = MultiByteToUnicodeString(p, CP_UTF8);
      break;
    }
    case kpidIsDir:
      prop = false;
      break;
    case kpidSize:
      if (f.size >= 0) prop = (UInt64)f.size;
      break;
    case kpidMTime:
    {
      if (f.date != 0)
      {
        Int64 d = f.date;
        unsigned ss = (unsigned)(d % 100); d /= 100;
        unsigned mm = (unsigned)(d % 100); d /= 100;
        unsigned hh = (unsigned)(d % 100); d /= 100;
        unsigned dd = (unsigned)(d % 100); d /= 100;
        unsigned mo = (unsigned)(d % 100); d /= 100;
        unsigned yr = (unsigned)d;
        UInt64 secs;
        if (NTime::GetSecondsSince1601(yr, mo, dd, hh, mm, ss, secs))
        {
          UInt64 ft100 = secs * 10000000ULL;
          FILETIME ft;
          ft.dwLowDateTime = (DWORD)ft100;
          ft.dwHighDateTime = (DWORD)(ft100 >> 32);
          prop.SetAsTimeFrom_FT_Prec(ft, k_PropVar_TimePrec_Base);
        }
      }
      break;
    }
    case kpidAttrib:
      if (f.attr != 0) prop = (UInt32)f.attr;
      break;
    case kpidComment:
      if (f.isDeletedMarker) prop = "deleted";
      break;
  }
  prop.Detach(value);
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *))
{
  COM_TRY_BEGIN
  Close();
  HRESULT res = _idx.Read(stream);
  if (res != S_OK)
    return res; // S_FALSE = not our format, real errors propagate
  _stream = stream;
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _idx.Clear();
  _stream.Release();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  (void)indices; (void)numItems; (void)testMode; (void)extractCallback;

  // TODO: for each requested file index
  //   1) collect its fragment list from _idx.files[i].frags
  //   2) group fragments by containing data block (_idx.dataBlocks[])
  //   3) for each data block: seek _stream, drive libzpaq::Decompresser
  //      block-by-block, splice fragment ranges to the output stream
  //   4) reorder spliced fragments back to the file's original order
  //
  // Until libzpaq is linked in, surface this clearly instead of silently
  // succeeding with empty output.
  return E_NOTIMPL;
  COM_TRY_END
}

// Two valid leading magics: "7kSt" (block) or "zPQ" + version byte (streaming).
API_FUNC_static_IsArc IsArc_Zpaq(const Byte *p, size_t size)
{
  if (size < 4)
    return k_IsArc_Res_NEED_MORE;
  if (memcmp(p, kZpaqBlockMagic, 4) == 0)
    return k_IsArc_Res_YES;
  if (size >= 4 && p[0] == 'z' && p[1] == 'P' && p[2] == 'Q' && p[3] >= 1)
    return k_IsArc_Res_YES;
  return k_IsArc_Res_NO;
}
}

static const Byte k_Signature[] = { 0x37, 0x6B, 0x53, 0x74 };

REGISTER_ARC_I(
  "zpaq", "zpaq", NULL, 0xE0,
  k_Signature,
  0,
  0,
  IsArc_Zpaq)

}}
