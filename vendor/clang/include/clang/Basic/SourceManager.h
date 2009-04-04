//===--- SourceManager.h - Track and cache source files ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the SourceManager interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SOURCEMANAGER_H
#define LLVM_CLANG_SOURCEMANAGER_H

#include "clang/Basic/SourceLocation.h"
#include "llvm/Bitcode/SerializationFwd.h"
#include "llvm/Support/DataTypes.h"
#include <vector>
#include <set>
#include <list>
#include <cassert>

namespace llvm {
class MemoryBuffer;
}
  
namespace clang {
  
class SourceManager;
class FileManager;
class FileEntry;
class IdentifierTokenInfo;
class LineTableInfo;

/// SrcMgr - Public enums and private classes that are part of the
/// SourceManager implementation.
///
namespace SrcMgr {
  /// CharacteristicKind - This is used to represent whether a file or directory
  /// holds normal user code, system code, or system code which is implicitly
  /// 'extern "C"' in C++ mode.  Entire directories can be tagged with this
  /// (this is maintained by DirectoryLookup and friends) as can specific
  /// FileIDInfos when a #pragma system_header is seen or various other cases.
  ///
  enum CharacteristicKind {
    C_User, C_System, C_ExternCSystem
  };
  
  /// ContentCache - Once instance of this struct is kept for every file
  /// loaded or used.  This object owns the MemoryBuffer object.
  class ContentCache {
    /// Buffer - The actual buffer containing the characters from the input
    /// file.  This is owned by the ContentCache object.
    mutable const llvm::MemoryBuffer *Buffer;

  public:
    /// Reference to the file entry.  This reference does not own
    /// the FileEntry object.  It is possible for this to be NULL if
    /// the ContentCache encapsulates an imaginary text buffer.
    const FileEntry *Entry;
    
    /// SourceLineCache - A new[]'d array of offsets for each source line.  This
    /// is lazily computed.  This is owned by the ContentCache object.
    unsigned *SourceLineCache;
    
    /// NumLines - The number of lines in this ContentCache.  This is only valid
    /// if SourceLineCache is non-null.
    unsigned NumLines;
    
    /// getBuffer - Returns the memory buffer for the associated content.
    const llvm::MemoryBuffer *getBuffer() const;
    
    /// getSize - Returns the size of the content encapsulated by this
    ///  ContentCache. This can be the size of the source file or the size of an
    ///  arbitrary scratch buffer.  If the ContentCache encapsulates a source
    ///  file this size is retrieved from the file's FileEntry.
    unsigned getSize() const;
    
    /// getSizeBytesMapped - Returns the number of bytes actually mapped for
    ///  this ContentCache.  This can be 0 if the MemBuffer was not actually
    ///  instantiated.
    unsigned getSizeBytesMapped() const;
    
    void setBuffer(const llvm::MemoryBuffer *B) {
      assert(!Buffer && "MemoryBuffer already set.");
      Buffer = B;
    }
        
    ContentCache(const FileEntry *e = NULL)
      : Buffer(NULL), Entry(e), SourceLineCache(NULL), NumLines(0) {}

    ~ContentCache();
    
    /// The copy ctor does not allow copies where source object has either
    ///  a non-NULL Buffer or SourceLineCache.  Ownership of allocated memory
    ///  is not transfered, so this is a logical error.
    ContentCache(const ContentCache &RHS) : Buffer(NULL),SourceLineCache(NULL) {
      Entry = RHS.Entry;

      assert (RHS.Buffer == NULL && RHS.SourceLineCache == NULL
              && "Passed ContentCache object cannot own a buffer.");
              
      NumLines = RHS.NumLines;      
    }
    
    /// Emit - Emit this ContentCache to Bitcode.
    void Emit(llvm::Serializer &S) const;
    
    /// ReadToSourceManager - Reconstitute a ContentCache from Bitcode
    //   and store it in the specified SourceManager.
    static void ReadToSourceManager(llvm::Deserializer &D, SourceManager &SM,
                                    FileManager *FMgr, std::vector<char> &Buf);
    
  private:
    // Disable assignments.
    ContentCache &operator=(const ContentCache& RHS);    
  };  

  /// FileInfo - Information about a FileID, basically just the logical file
  /// that it represents and include stack information.
  ///
  /// Each FileInfo has include stack information, indicating where it came
  /// from.  This information encodes the #include chain that a token was
  /// instantiated from.  The main include file has an invalid IncludeLoc.
  ///
  /// FileInfos contain a "ContentCache *", with the contents of the file.
  ///
  class FileInfo {
    /// IncludeLoc - The location of the #include that brought in this file.
    /// This is an invalid SLOC for the main file (top of the #include chain).
    unsigned IncludeLoc;  // Really a SourceLocation
    
    /// Data - This contains the ContentCache* and the bits indicating the
    /// characteristic of the file and whether it has #line info, all bitmangled
    /// together.
    uintptr_t Data;
  public:
    /// get - Return a FileInfo object.
    static FileInfo get(SourceLocation IL, const ContentCache *Con,
                        CharacteristicKind FileCharacter) {
      FileInfo X;
      X.IncludeLoc = IL.getRawEncoding();
      X.Data = (uintptr_t)Con;
      assert((X.Data & 3) == 0 &&"ContentCache pointer insufficiently aligned");
      assert((unsigned)FileCharacter < 4 && "invalid file character");
      X.Data |= (unsigned)FileCharacter;
      return X;
    }
    
    SourceLocation getIncludeLoc() const {
      return SourceLocation::getFromRawEncoding(IncludeLoc);
    }
    const ContentCache* getContentCache() const {
      return reinterpret_cast<const ContentCache*>(Data & ~3UL);
    }
    
    /// getCharacteristic - Return whether this is a system header or not.
    CharacteristicKind getFileCharacteristic() const { 
      return (CharacteristicKind)(Data & 3);
    }
  };
  
  /// InstantiationInfo - Each InstantiationInfo encodes the Instantiation
  /// location - where the token was ultimately instantiated, and the
  /// SpellingLoc - where the actual character data for the token came from.
  class InstantiationInfo {
    unsigned InstantiationLoc, SpellingLoc; // Really these are SourceLocations.
  public:
    SourceLocation getInstantiationLoc() const {
      return SourceLocation::getFromRawEncoding(InstantiationLoc);
    }
    SourceLocation getSpellingLoc() const {
      return SourceLocation::getFromRawEncoding(SpellingLoc);
    }
    
    /// get - Return a InstantiationInfo for an expansion.  VL specifies
    /// the instantiation location (where the macro is expanded), and SL
    /// specifies the spelling location (where the characters from the token
    /// come from).  Both VL and PL refer to normal File SLocs.
    static InstantiationInfo get(SourceLocation IL, SourceLocation SL) {
      InstantiationInfo X;
      X.InstantiationLoc = IL.getRawEncoding();
      X.SpellingLoc = SL.getRawEncoding();
      return X;
    }
  };
  
  /// SLocEntry - This is a discriminated union of FileInfo and
  /// InstantiationInfo.  SourceManager keeps an array of these objects, and
  /// they are uniquely identified by the FileID datatype.
  class SLocEntry {
    unsigned Offset;   // low bit is set for instantiation info.
    union {
      FileInfo File;
      InstantiationInfo Instantiation;
    };
  public:
    unsigned getOffset() const { return Offset >> 1; }
    
    bool isInstantiation() const { return Offset & 1; }
    bool isFile() const { return !isInstantiation(); }
    
    const FileInfo &getFile() const {
      assert(isFile() && "Not a file SLocEntry!");
      return File;
    }

    const InstantiationInfo &getInstantiation() const {
      assert(isInstantiation() && "Not an instantiation SLocEntry!");
      return Instantiation;
    }
    
    static SLocEntry get(unsigned Offset, const FileInfo &FI) {
      SLocEntry E;
      E.Offset = Offset << 1;
      E.File = FI;
      return E;
    }

    static SLocEntry get(unsigned Offset, const InstantiationInfo &II) {
      SLocEntry E;
      E.Offset = (Offset << 1) | 1;
      E.Instantiation = II;
      return E;
    }
  };
}  // end SrcMgr namespace.
} // end clang namespace

namespace std {
template <> struct less<clang::SrcMgr::ContentCache> {
  inline bool operator()(const clang::SrcMgr::ContentCache& L,
                         const clang::SrcMgr::ContentCache& R) const {
    return L.Entry < R.Entry;
  }
};
} // end std namespace

namespace clang {
  
/// SourceManager - This file handles loading and caching of source files into
/// memory.  This object owns the MemoryBuffer objects for all of the loaded
/// files and assigns unique FileID's for each unique #include chain.
///
/// The SourceManager can be queried for information about SourceLocation
/// objects, turning them into either spelling or instantiation locations.
/// Spelling locations represent where the bytes corresponding to a token came
/// from and instantiation locations represent where the location is in the
/// user's view.  In the case of a macro expansion, for example, the spelling
/// location indicates where the expanded token came from and the instantiation
/// location specifies where it was expanded.
class SourceManager {
  /// FileInfos - Memoized information about all of the files tracked by this
  /// SourceManager.  This set allows us to merge ContentCache entries based
  /// on their FileEntry*.  All ContentCache objects will thus have unique,
  /// non-null, FileEntry pointers.  
  std::set<SrcMgr::ContentCache> FileInfos;
  
  /// MemBufferInfos - Information about various memory buffers that we have
  /// read in.  This is a list, instead of a vector, because we need pointers to
  /// the ContentCache objects to be stable.  All FileEntry* within the
  /// stored ContentCache objects are NULL, as they do not refer to a file.
  std::list<SrcMgr::ContentCache> MemBufferInfos;
  
  /// SLocEntryTable - This is an array of SLocEntry's that we have created.
  /// FileID is an index into this vector.  This array is sorted by the offset.
  std::vector<SrcMgr::SLocEntry> SLocEntryTable;
  /// NextOffset - This is the next available offset that a new SLocEntry can
  /// start at.  It is SLocEntryTable.back().getOffset()+size of back() entry.
  unsigned NextOffset;
  
  /// LastFileIDLookup - This is a one-entry cache to speed up getFileID.
  /// LastFileIDLookup records the last FileID looked up or created, because it
  /// is very common to look up many tokens from the same file.
  mutable FileID LastFileIDLookup;
  
  /// LineTable - This holds information for #line directives.  It is referenced
  /// by indices from SLocEntryTable.
  LineTableInfo *LineTable;
  
  /// LastLineNo - These ivars serve as a cache used in the getLineNumber
  /// method which is used to speedup getLineNumber calls to nearby locations.
  mutable FileID LastLineNoFileIDQuery;
  mutable SrcMgr::ContentCache *LastLineNoContentCache;
  mutable unsigned LastLineNoFilePos;
  mutable unsigned LastLineNoResult;
  
  /// MainFileID - The file ID for the main source file of the translation unit.
  FileID MainFileID;

  // Statistics for -print-stats.
  mutable unsigned NumLinearScans, NumBinaryProbes;
  
  // SourceManager doesn't support copy construction.
  explicit SourceManager(const SourceManager&);
  void operator=(const SourceManager&);  
public:
  SourceManager() : LineTable(0), NumLinearScans(0), NumBinaryProbes(0) {
    clearIDTables();
  }
  ~SourceManager();
  
  void clearIDTables();
  
  //===--------------------------------------------------------------------===//
  // MainFileID creation and querying methods.
  //===--------------------------------------------------------------------===//

  /// getMainFileID - Returns the FileID of the main source file.
  FileID getMainFileID() const { return MainFileID; }
  
  /// createMainFileID - Create the FileID for the main source file.
  FileID createMainFileID(const FileEntry *SourceFile,
                          SourceLocation IncludePos) {
    assert(MainFileID.isInvalid() && "MainFileID already set!");
    MainFileID = createFileID(SourceFile, IncludePos, SrcMgr::C_User);
    return MainFileID;
  }
  
  //===--------------------------------------------------------------------===//
  // Methods to create new FileID's and instantiations.
  //===--------------------------------------------------------------------===//
  
  /// createFileID - Create a new FileID that represents the specified file
  /// being #included from the specified IncludePosition.  This returns 0 on
  /// error and translates NULL into standard input.
  FileID createFileID(const FileEntry *SourceFile, SourceLocation IncludePos,
                      SrcMgr::CharacteristicKind FileCharacter) {
    const SrcMgr::ContentCache *IR = getOrCreateContentCache(SourceFile);
    if (IR == 0) return FileID();    // Error opening file?
    return createFileID(IR, IncludePos, FileCharacter);
  }
  
  /// createFileIDForMemBuffer - Create a new FileID that represents the
  /// specified memory buffer.  This does no caching of the buffer and takes
  /// ownership of the MemoryBuffer, so only pass a MemoryBuffer to this once.
  FileID createFileIDForMemBuffer(const llvm::MemoryBuffer *Buffer) {
    return createFileID(createMemBufferContentCache(Buffer), SourceLocation(),
                        SrcMgr::C_User);
  }
  
  /// createMainFileIDForMembuffer - Create the FileID for a memory buffer
  ///  that will represent the FileID for the main source.  One example
  ///  of when this would be used is when the main source is read from STDIN.
  FileID createMainFileIDForMemBuffer(const llvm::MemoryBuffer *Buffer) {
    assert(MainFileID.isInvalid() && "MainFileID already set!");
    MainFileID = createFileIDForMemBuffer(Buffer);
    return MainFileID;
  }

  /// createInstantiationLoc - Return a new SourceLocation that encodes the fact
  /// that a token at Loc should actually be referenced from InstantiationLoc.
  /// TokLength is the length of the token being instantiated.
  SourceLocation createInstantiationLoc(SourceLocation Loc,
                                        SourceLocation InstantiationLoc,
                                        unsigned TokLength);
  
  //===--------------------------------------------------------------------===//
  // FileID manipulation methods.
  //===--------------------------------------------------------------------===//
  
  /// getBuffer - Return the buffer for the specified FileID.
  ///
  const llvm::MemoryBuffer *getBuffer(FileID FID) const {
    return getSLocEntry(FID).getFile().getContentCache()->getBuffer();
  }
  
  /// getFileEntryForID - Returns the FileEntry record for the provided FileID.
  const FileEntry *getFileEntryForID(FileID FID) const {
    return getSLocEntry(FID).getFile().getContentCache()->Entry;
  }
  
  /// getBufferData - Return a pointer to the start and end of the source buffer
  /// data for the specified FileID.
  std::pair<const char*, const char*> getBufferData(FileID FID) const;
  
  
  //===--------------------------------------------------------------------===//
  // SourceLocation manipulation methods.
  //===--------------------------------------------------------------------===//
  
  /// getFileIDSlow - Return the FileID for a SourceLocation.  This is a very
  /// hot method that is used for all SourceManager queries that start with a
  /// SourceLocation object.  It is responsible for finding the entry in
  /// SLocEntryTable which contains the specified location.
  ///
  FileID getFileID(SourceLocation SpellingLoc) const {
    unsigned SLocOffset = SpellingLoc.getOffset();
    
    // If our one-entry cache covers this offset, just return it.
    if (isOffsetInFileID(LastFileIDLookup, SLocOffset))
      return LastFileIDLookup;

    return getFileIDSlow(SLocOffset);
  }
  
  /// getLocForStartOfFile - Return the source location corresponding to the
  /// first byte of the specified file.
  SourceLocation getLocForStartOfFile(FileID FID) const {
    assert(FID.ID < SLocEntryTable.size() && SLocEntryTable[FID.ID].isFile());
    unsigned FileOffset = SLocEntryTable[FID.ID].getOffset();
    return SourceLocation::getFileLoc(FileOffset);
  }
  
  /// Given a SourceLocation object, return the instantiation location
  /// referenced by the ID.
  SourceLocation getInstantiationLoc(SourceLocation Loc) const {
    // Handle the non-mapped case inline, defer to out of line code to handle
    // instantiations.
    if (Loc.isFileID()) return Loc;
    return getInstantiationLocSlowCase(Loc);
  }
  
  /// getSpellingLoc - Given a SourceLocation object, return the spelling
  /// location referenced by the ID.  This is the place where the characters
  /// that make up the lexed token can be found.
  SourceLocation getSpellingLoc(SourceLocation Loc) const {
    // Handle the non-mapped case inline, defer to out of line code to handle
    // instantiations.
    if (Loc.isFileID()) return Loc;
    return getSpellingLocSlowCase(Loc);
  }

  /// getDecomposedLoc - Decompose the specified location into a raw FileID +
  /// Offset pair.  The first element is the FileID, the second is the
  /// offset from the start of the buffer of the location.
  std::pair<FileID, unsigned> getDecomposedLoc(SourceLocation Loc) const {
    FileID FID = getFileID(Loc);
    return std::make_pair(FID, Loc.getOffset()-getSLocEntry(FID).getOffset());
  }
  
  /// getDecomposedInstantiationLoc - Decompose the specified location into a
  /// raw FileID + Offset pair.  If the location is an instantiation record,
  /// walk through it until we find the final location instantiated.
  std::pair<FileID, unsigned>
  getDecomposedInstantiationLoc(SourceLocation Loc) const {
    FileID FID = getFileID(Loc);
    const SrcMgr::SLocEntry *E = &getSLocEntry(FID);
    
    unsigned Offset = Loc.getOffset()-E->getOffset();
    if (Loc.isFileID())
      return std::make_pair(FID, Offset);
    
    return getDecomposedInstantiationLocSlowCase(E, Offset);
  }

  /// getDecomposedSpellingLoc - Decompose the specified location into a raw
  /// FileID + Offset pair.  If the location is an instantiation record, walk
  /// through it until we find its spelling record.
  std::pair<FileID, unsigned>
  getDecomposedSpellingLoc(SourceLocation Loc) const {
    FileID FID = getFileID(Loc);
    const SrcMgr::SLocEntry *E = &getSLocEntry(FID);
    
    unsigned Offset = Loc.getOffset()-E->getOffset();
    if (Loc.isFileID())
      return std::make_pair(FID, Offset);
    return getDecomposedSpellingLocSlowCase(E, Offset);
  }    
  
  /// getFileOffset - This method returns the offset from the start
  /// of the file that the specified SourceLocation represents. This is not very
  /// meaningful for a macro ID.
  unsigned getFileOffset(SourceLocation SpellingLoc) const {
    return getDecomposedLoc(SpellingLoc).second;
  }
  
  
  //===--------------------------------------------------------------------===//
  // Queries about the code at a SourceLocation.
  //===--------------------------------------------------------------------===//
  
  /// getCharacterData - Return a pointer to the start of the specified location
  /// in the appropriate spelling MemoryBuffer.
  const char *getCharacterData(SourceLocation SL) const;
  
  /// getColumnNumber - Return the column # for the specified file position.
  /// This is significantly cheaper to compute than the line number.  This
  /// returns zero if the column number isn't known.  This may only be called on
  /// a file sloc, so you must choose a spelling or instantiation location
  /// before calling this method.
  unsigned getColumnNumber(SourceLocation Loc) const;
  
  unsigned getSpellingColumnNumber(SourceLocation Loc) const {
    return getColumnNumber(getSpellingLoc(Loc));
  }
  unsigned getInstantiationColumnNumber(SourceLocation Loc) const {
    return getColumnNumber(getInstantiationLoc(Loc));
  }
  
  
  /// getLineNumber - Given a SourceLocation, return the spelling line number
  /// for the position indicated.  This requires building and caching a table of
  /// line offsets for the MemoryBuffer, so this is not cheap: use only when
  /// about to emit a diagnostic.
  unsigned getLineNumber(SourceLocation Loc) const;
  
  unsigned getInstantiationLineNumber(SourceLocation Loc) const {
    return getLineNumber(getInstantiationLoc(Loc));
  }
  unsigned getSpellingLineNumber(SourceLocation Loc) const {
    return getLineNumber(getSpellingLoc(Loc));
  }
  
  // FIXME: This should handle #line.
  SrcMgr::CharacteristicKind getFileCharacteristic(SourceLocation Loc) const {
    FileID FID = getFileID(getSpellingLoc(Loc));
    return getSLocEntry(FID).getFile().getFileCharacteristic();
  }
  
  /// getPresumedLoc - This method returns the "presumed" location of a
  /// SourceLocation specifies.  A "presumed location" can be modified by #line
  /// or GNU line marker directives.  This provides a view on the data that a
  /// user should see in diagnostics, for example.
  ///
  /// Note that a presumed location is always given as the instantiation point
  /// of an instantiation location, not at the spelling location.
  PresumedLoc getPresumedLoc(SourceLocation Loc) const;
  
  
  
  /// isFromSameFile - Returns true if both SourceLocations correspond to
  ///  the same file.
  bool isFromSameFile(SourceLocation Loc1, SourceLocation Loc2) const {
    return getFileID(Loc1) == getFileID(Loc2);
  }
  
  /// isFromMainFile - Returns true if the file of provided SourceLocation is
  ///   the main file.
  bool isFromMainFile(SourceLocation Loc) const {
    return getFileID(Loc) == getMainFileID();
  } 
  
  /// isInSystemHeader - Returns if a SourceLocation is in a system header.
  bool isInSystemHeader(SourceLocation Loc) const {
    return getFileCharacteristic(Loc) != SrcMgr::C_User;
  }
  
  //===--------------------------------------------------------------------===//
  // Line Table Manipulation Routines
  //===--------------------------------------------------------------------===//
  
  /// getLineTableFilenameID - Return the uniqued ID for the specified filename.
  /// 
  unsigned getLineTableFilenameID(const char *Ptr, unsigned Len);
  
  
  //===--------------------------------------------------------------------===//
  // Other miscellaneous methods.
  //===--------------------------------------------------------------------===//
  
  // Iterators over FileInfos.
  typedef std::set<SrcMgr::ContentCache>::const_iterator fileinfo_iterator;
  fileinfo_iterator fileinfo_begin() const { return FileInfos.begin(); }
  fileinfo_iterator fileinfo_end() const { return FileInfos.end(); }

  /// PrintStats - Print statistics to stderr.
  ///
  void PrintStats() const;

  /// Emit - Emit this SourceManager to Bitcode.
  void Emit(llvm::Serializer& S) const;
  
  /// Read - Reconstitute a SourceManager from Bitcode.
  static SourceManager* CreateAndRegister(llvm::Deserializer& S,
                                          FileManager &FMgr);
  
private:
  friend struct SrcMgr::ContentCache; // Used for deserialization.
  
  /// isOffsetInFileID - Return true if the specified FileID contains the
  /// specified SourceLocation offset.  This is a very hot method.
  inline bool isOffsetInFileID(FileID FID, unsigned SLocOffset) const {
    const SrcMgr::SLocEntry &Entry = getSLocEntry(FID);
    // If the entry is after the offset, it can't contain it.
    if (SLocOffset < Entry.getOffset()) return false;
    
    // If this is the last entry than it does.  Otherwise, the entry after it
    // has to not include it.
    if (FID.ID+1 == SLocEntryTable.size()) return true;
    return SLocOffset < SLocEntryTable[FID.ID+1].getOffset();
  }
  
  /// createFileID - Create a new fileID for the specified ContentCache and
  ///  include position.  This works regardless of whether the ContentCache
  ///  corresponds to a file or some other input source.
  FileID createFileID(const SrcMgr::ContentCache* File,
                      SourceLocation IncludePos,
                      SrcMgr::CharacteristicKind DirCharacter);
    
  const SrcMgr::ContentCache *
    getOrCreateContentCache(const FileEntry *SourceFile);

  /// createMemBufferContentCache - Create a new ContentCache for the specified
  ///  memory buffer.
  const SrcMgr::ContentCache* 
  createMemBufferContentCache(const llvm::MemoryBuffer *Buf);

  const SrcMgr::SLocEntry &getSLocEntry(FileID FID) const {
    assert(FID.ID < SLocEntryTable.size() && "Invalid id");
    return SLocEntryTable[FID.ID];
  }
  
  FileID getFileIDSlow(unsigned SLocOffset) const;

  SourceLocation getInstantiationLocSlowCase(SourceLocation Loc) const;
  SourceLocation getSpellingLocSlowCase(SourceLocation Loc) const;

  std::pair<FileID, unsigned>
  getDecomposedInstantiationLocSlowCase(const SrcMgr::SLocEntry *E,
                                        unsigned Offset) const;  
  std::pair<FileID, unsigned>
  getDecomposedSpellingLocSlowCase(const SrcMgr::SLocEntry *E,
                                   unsigned Offset) const;
};


}  // end namespace clang

#endif