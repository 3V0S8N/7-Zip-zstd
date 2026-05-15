// ZpaqIn.cpp -- ZPAQ journaling archive index parser

#include "StdAfx.h"

#include "../../Common/StreamUtils.h"

#include "ZpaqIn.h"

namespace NArchive {
namespace NZpaq {

void CIndex::Clear()
{
  versions.Clear();
  files.Clear();
  fragments.Clear();
  dataBlocks.Clear();
  isStreaming = false;
  archiveSize = 0;
}

HRESULT CIndex::Read(IInStream *stream)
{
  Clear();

  UInt64 endPos = 0;
  RINOK(stream->Seek(0, STREAM_SEEK_END, &endPos))
  archiveSize = (Int64)endPos;
  RINOK(stream->Seek(0, STREAM_SEEK_SET, NULL))

  Byte head[4];
  RINOK(ReadStream_FALSE(stream, head, 4))
  if (memcmp(head, kZpaqBlockMagic, 4) != 0)
    return S_FALSE;

  // TODO: drive libzpaq::Decompresser over `stream` and for each block:
  //   - identify segment kind by filename prefix:
  //       "jDC<date>c<num>" -> C block: open a new CVersion
  //       "jDC<date>d<num>" -> D block: append CDataBlock, remember offset
  //       "jDC<date>h<num>" -> H block: parse fragment hashes (HT)
  //       "jDC<date>i<num>" -> I block: parse file index entries (DT)
  //     (date = 14 digits, num = 10 digits, per ZPAQ spec)
  //   - if no jDC* segments at all, mark isStreaming and synthesize one
  //     CVersion containing every streaming segment as a CFile.
  //
  // Cross-version semantics for the visible file tree:
  //   - Append one CFile per (version, path) so each version shows its own
  //     snapshot. Deletes get isDeletedMarker=true so the handler can skip
  //     them from the listing while still being able to surface them via
  //     a future kpidComment "deleted in vNNNN" entry.
  //
  // For now this is a stub: returning S_FALSE keeps Open() rejecting the
  // archive so we never claim partial support.

  return S_FALSE;
}

}}
