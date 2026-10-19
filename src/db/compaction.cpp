#include "db/compaction.h"
#include "db/db_impl.h"
#include "db/filename.h"
#include "table/table_builder.h"
#include "util/coding.h"
#include <chrono>

namespace lsmforge {

Status CompactionEngine::DoCompactionWork(DBImpl* db, Compaction* c) {
    const uint64_t start_time = db->options().comparator ? Env::Default()->NowMicros() : 0;

    if (c->IsTrivialMove()) {
        // Move file directly from level to level+1 without re-encoding
        FileMetaData* f = c->input(0, 0);
        c->edit()->DeleteFile(c->level(), f->number);
        c->edit()->AddFile(c->level() + 1, f->number, f->file_size, f->smallest, f->largest);
        Status s = db->versions()->LogAndApply(c->edit(), &db->mutex());
        c->ReleaseInputs();
        return s;
    }

    uint64_t bytes_read = 0;
    for (int which = 0; which < 2; which++) {
        for (int i = 0; i < c->num_input_files(which); i++) {
            bytes_read += c->input(which, i)->file_size;
        }
    }
    db->stats().compaction_bytes_read.fetch_add(bytes_read, std::memory_order_relaxed);

    std::unique_ptr<Iterator> input(db->versions()->MakeInputIterator(c));

    // Release mutex while doing actual I/O
    db->mutex().unlock();

    input->SeekToFirst();
    Status status;

    std::string current_user_key;
    bool has_current_user_key = false;
    SequenceNumber last_sequence_for_key = kMaxSequenceNumber;

    const SequenceNumber earliest_snapshot = db->GetEarliestSnapshot();
    const Comparator* user_cmp = db->versions()->icmp().user_comparator();

    std::unique_ptr<WritableFile> outfile;
    std::unique_ptr<TableBuilder> builder;
    uint64_t current_file_number = 0;
    uint64_t bytes_written = 0;

    auto FinishOutputFile = [&]() -> Status {
        if (builder != nullptr) {
            Status s = builder->Finish();
            if (s.ok()) {
                const uint64_t current_entries = builder->NumEntries();
                const uint64_t current_bytes = builder->FileSize();
                bytes_written += current_bytes;
                if (current_entries > 0) {
                    c->edit()->AddFile(c->level() + 1, current_file_number, current_bytes,
                                       builder->SmallestKey(), builder->LargestKey());
                }
            }
            if (s.ok()) {
                s = outfile->Sync();
            }
            if (s.ok()) {
                s = outfile->Close();
            }
            if (s.ok()) {
                std::string tmp_name = TempFileName(db->dbname(), current_file_number);
                std::string sst_name = TableFileName(db->dbname(), current_file_number);
                s = Env::Default()->RenameFile(tmp_name, sst_name);
            }
            builder.reset();
            outfile.reset();
            return s;
        }
        return Status::OK();
    };

    while (input->Valid()) {
        Slice key = input->key();
        Slice value = input->value();

        ParsedInternalKey ikey;
        if (!ParseInternalKey(key, &ikey)) {
            status = Status::Corruption("corrupted key in compaction");
            break;
        }

        // Check if this key is different from the previous user key
        if (!has_current_user_key || user_cmp->Compare(ikey.user_key, current_user_key) != 0) {
            // First occurrence of this user key
            current_user_key.assign(ikey.user_key.data(), ikey.user_key.size());
            has_current_user_key = true;
            last_sequence_for_key = kMaxSequenceNumber;
        }

        bool drop = false;
        if (last_sequence_for_key <= earliest_snapshot) {
            // Newer version of this key was already emitted at a snapshot >= earliest_snapshot.
            // This older version is obsolete!
            drop = true;
        } else if (ikey.type == kTypeDeletion &&
                   ikey.sequence <= earliest_snapshot &&
                   c->IsBaseLevelForKey(ikey.user_key)) {
            // For this user key:
            // (1) There is no older data in higher levels
            // (2) The deletion tombstone has sequence <= earliest_snapshot
            // Therefore, this deletion tombstone can be dropped completely!
            drop = true;
        }

        last_sequence_for_key = ikey.sequence;

        if (!drop) {
            // Open new output file if needed
            if (builder == nullptr) {
                db->mutex().lock();
                current_file_number = db->versions()->NewFileNumber();
                db->mutex().unlock();

                std::string tmp_name = TempFileName(db->dbname(), current_file_number);
                status = Env::Default()->NewWritableFile(tmp_name, &outfile);
                Options table_options = db->options();
                table_options.comparator = &db->versions()->icmp();
                builder = std::make_unique<TableBuilder>(table_options, outfile.get());
            }

            builder->Add(key, value);

            if (builder->FileSize() >= c->MaxOutputFileSize()) {
                status = FinishOutputFile();
                if (!status.ok()) break;
            }
        }

        input->Next();
    }

    if (status.ok()) {
        status = input->status();
    }
    if (status.ok()) {
        status = FinishOutputFile();
    }

    db->mutex().lock();

    if (status.ok()) {
        c->AddInputDeletions(c->edit());
        status = db->versions()->LogAndApply(c->edit(), &db->mutex());
    }

    c->ReleaseInputs();

    db->stats().compaction_bytes_written.fetch_add(bytes_written, std::memory_order_relaxed);
    db->stats().compaction_count.fetch_add(1, std::memory_order_relaxed);
    if (start_time > 0) {
        uint64_t duration = Env::Default()->NowMicros() - start_time;
        db->stats().compaction_duration_micros.fetch_add(duration, std::memory_order_relaxed);
    }

    return status;
}

} // namespace lsmforge
