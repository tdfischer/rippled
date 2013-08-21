// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_MEMTABLE_H_
#define STORAGE_LEVELDB_DB_MEMTABLE_H_

#include <string>
#include <memory>
#include <deque>
#include "db/dbformat.h"
#include "db/skiplist.h"
#include "db/version_set.h"
#include "leveldb/db.h"
#include "leveldb/memtablerep.h"
#include "util/arena_impl.h"

namespace leveldb {

class Mutex;
class MemTableIterator;

class MemTable {
 public:
  struct KeyComparator : public MemTableRep::KeyComparator {
    const InternalKeyComparator comparator;
    explicit KeyComparator(const InternalKeyComparator& c) : comparator(c) { }
    virtual int operator()(const char* a, const char* b) const;
  };

  // MemTables are reference counted.  The initial reference count
  // is zero and the caller must call Ref() at least once.
  explicit MemTable(
    const InternalKeyComparator& comparator,
    std::shared_ptr<MemTableRepFactory> table_factory,
    int numlevel = 7,
    const Options& options = Options());

  // Increase reference count.
  void Ref() { ++refs_; }

  // Drop reference count.  Delete if no more references exist.
  void Unref() {
    --refs_;
    assert(refs_ >= 0);
    if (refs_ <= 0) {
      delete this;
    }
  }

  // Returns an estimate of the number of bytes of data in use by this
  // data structure.
  //
  // REQUIRES: external synchronization to prevent simultaneous
  // operations on the same MemTable.
  size_t ApproximateMemoryUsage();

  // Return an iterator that yields the contents of the memtable.
  //
  // The caller must ensure that the underlying MemTable remains live
  // while the returned iterator is live.  The keys returned by this
  // iterator are internal keys encoded by AppendInternalKey in the
  // db/dbformat.{h,cc} module.
  Iterator* NewIterator();

  // Add an entry into memtable that maps key to value at the
  // specified sequence number and with the specified type.
  // Typically value will be empty if type==kTypeDeletion.
  void Add(SequenceNumber seq, ValueType type,
           const Slice& key,
           const Slice& value);

  // If memtable contains a value for key, store it in *value and return true.
  // If memtable contains a deletion for key, store a NotFound() error
  // in *status and return true.
  // If memtable contains Merge operation as the most recent entry for a key,
  //   and the merge process does not stop (not reaching a value or delete),
  //   prepend the current merge operand to *operands.
  //   store MergeInProgress in s, and return false.
  // Else, return false.
  bool Get(const LookupKey& key, std::string* value, Status* s,
           std::deque<std::string>* operands, const Options& options);

  // Returns the edits area that is needed for flushing the memtable
  VersionEdit* GetEdits() { return &edit_; }

  // Returns the sequence number of the first element that was inserted
  // into the memtable
  SequenceNumber GetFirstSequenceNumber() { return first_seqno_; }

  // Returns the logfile number that can be safely deleted when this
  // memstore is flushed to storage
  uint64_t GetLogNumber() { return mem_logfile_number_; }

  // Sets the logfile number that can be safely deleted when this
  // memstore is flushed to storage
  void SetLogNumber(uint64_t num) { mem_logfile_number_ = num; }

 private:
  ~MemTable();  // Private since only Unref() should be used to delete it
  friend class MemTableIterator;
  friend class MemTableBackwardIterator;
  friend class MemTableList;

  KeyComparator comparator_;
  int refs_;
  ArenaImpl arena_impl_;
  shared_ptr<MemTableRep> table_;

  // These are used to manage memtable flushes to storage
  bool flush_in_progress_; // started the flush
  bool flush_completed_;   // finished the flush
  uint64_t file_number_;    // filled up after flush is complete

  // The udpates to be applied to the transaction log when this
  // memtable is flushed to storage.
  VersionEdit edit_;

  // The sequence number of the kv that was inserted first
  SequenceNumber first_seqno_;

  // The log files earlier than this number can be deleted.
  uint64_t mem_logfile_number_;

  // No copying allowed
  MemTable(const MemTable&);
  void operator=(const MemTable&);
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_MEMTABLE_H_