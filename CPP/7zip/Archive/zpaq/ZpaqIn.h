// ZpaqIn.h -- ZPAQ journaling archive index parser

#ifndef ZIP7_INC_ZPAQ_IN_H
#define ZIP7_INC_ZPAQ_IN_H

#include "../../../Common/MyBuffer.h"
#include "../../../Common/MyString.h"
#include "../../../Common/MyVector.h"

#include "../../IStream.h"

namespace NArchive {
namespace NZpaq {

// 4-byte ZPAQ block magic ("7kSt").
const Byte kZpaqBlockMagic[4] = { 0x37, 0x6B, 0x53, 0x74 };

// Fragment hash table entry (one fragment of compressed data).
struct CFragment
{
  Byte sha1[20];
  UInt32 usize;      // uncompressed size of fragment
  Int64 blockOffset; // offset of containing D block in archive, -1 if unresolved

  CFragment(): usize(0), blockOffset(-1) { memset(sha1, 0, 20); }
};

// One D block: a solid range of fragments compressed together.
struct CDataBlock
{
  Int64 offset;     // absolute file offset of block start
  Int64 csize;      // compressed size including header
  UInt32 firstFrag; // index into fragments[] of first fragment in this block
  UInt32 numFrags;  // count of fragments in this block

  CDataBlock(): offset(0), csize(0), firstFrag(0), numFrags(0) {}
};

// One transaction (= one user-visible "version" / "snapshot").
struct CVersion
{
  Int64 date;            // decimal YYYYMMDDHHMMSS (UT)
  Int64 cBlockOffset;    // offset of C (control) block
  Int64 dataOffset;      // offset of first D block of this transaction
  Int64 csize;           // total compressed bytes of this transaction
  UInt32 firstFragment;  // first fragment ID added in this version
  UInt32 firstFileIndex; // first index into files[] that belongs to this version
  UInt32 numFiles;       // number of files exposed for this version
  Int64 usize;           // uncompressed bytes added/changed in this version
  int updates;           // file updates (add or change)
  int deletes;           // file deletions

  CVersion(): date(0), cBlockOffset(0), dataOffset(0), csize(0),
              firstFragment(0), firstFileIndex(0), numFiles(0),
              usize(0), updates(0), deletes(0) {}
};

// One logical file as visible inside one version.
// Multiple versions referring to "the same" path produce multiple CFile entries.
struct CFile
{
  AString path;                // UTF-8 path inside archive (no version prefix)
  Int64 date;                  // decimal YYYYMMDDHHMMSS (UT), 0 = deleted marker
  Int64 size;                  // uncompressed size or -1 if unknown
  UInt64 attr;                 // first 8 attribute bytes
  CRecordVector<UInt32> frags; // fragment indices into CIndex::fragments
  UInt32 versionIndex;         // which CVersion this file belongs to
  bool isDeletedMarker;        // true if this entry just deletes a prior path

  CFile(): date(0), size(-1), attr(0), versionIndex(0), isDeletedMarker(false) {}
};

// Parsed in-memory index of one ZPAQ journaling archive.
// Streaming-only archives are exposed as a single synthetic version.
class CIndex
{
public:
  CObjectVector<CVersion> versions;
  CObjectVector<CFile> files;          // flat across all versions
  CRecordVector<CFragment> fragments;
  CObjectVector<CDataBlock> dataBlocks;

  bool isStreaming;                    // true if no C blocks found
  Int64 archiveSize;

  CIndex(): isStreaming(false), archiveSize(0) {}

  void Clear();

  // Walk the archive end-to-end and populate the vectors above.
  // Implementation will use libzpaq::Decompresser to iterate blocks/segments,
  // then re-interpret C/H/I segments per the ZPAQ journaling spec.
  HRESULT Read(IInStream *stream);
};

}}

#endif
