#include "db/version_set.h"
#include "db/filename.h"
#include "table/merging_iterator.h"
#include "util/coding.h"
#include "wal/wal_reader.h"
#include <algorithm>
#include <cassert>

namespace lsmforge {

namespace {

uint64_t MaxBytesForLevel(const Options* options, int level) {
    // Note: level 0 size limit is governed by file count, not bytes
    uint64_t result = options->max_bytes_for_level_base;
    while (level > 1) {
        result *= static_cast<uint64_t>(options->max_bytes_for_level_multiplier);
        level--;
    }
    return result;
}

int FindFile(const InternalKeyComparator& icmp,
             const std::vector<FileMetaData*>& files,
             const Slice& key) {
    uint32_t left = 0;
    uint32_t right = static_cast<uint32_t>(files.size());
    while (left < right) {
        uint32_t mid = (left + right) / 2;
        const FileMetaData* f = files[mid];
        if (icmp.Compare(f->largest, key) < 0) {
            // Key at "mid.largest" is < "target". Therefore all
            // files at or before "mid" are uninteresting.
            left = mid + 1;
        } else {
            // Key at "mid.largest" is >= "target". Therefore all files
            // after "mid" are uninteresting.
            right = mid;
        }
    }
    return static_cast<int>(right);
}

} // namespace

Version::Version(VersionSet* vset)
    : vset_(vset), next_(this), prev_(this), refs_(0) {}

Version::~Version() {
    assert(refs_ == 0);
    // Remove from linked list
    prev_->next_ = next_;
    next_->prev_ = prev_;

    // Drop references to files
    for (int level = 0; level < kNumLevels; level++) {
        for (auto* f : files_[level]) {
            assert(f->refs > 0);
            f->refs--;
            if (f->refs <= 0) {
                delete f;
            }
        }
    }
}

void Version::Ref() {
    ++refs_;
}

void Version::Unref() {
    assert(refs_ > 0);
    --refs_;
    if (refs_ == 0) {
        delete this;
    }
}

uint64_t Version::NumLevelBytes(int level) const {
    assert(level >= 0 && level < kNumLevels);
    uint64_t sum = 0;
    for (const auto* f : files_[level]) {
        sum += f->file_size;
    }
    return sum;
}

void Version::AddIterators(const ReadOptions& options, std::vector<Iterator*>* iters) {
    // Level-0 files can overlap each other, so each file needs an iterator
    for (const auto* f : files_[0]) {
        iters->push_back(vset_->table_cache()->NewIterator(options, f->number, f->file_size));
    }

    // For levels > 0, we can add individual iterators
    for (int level = 1; level < kNumLevels; level++) {
        for (const auto* f : files_[level]) {
            iters->push_back(vset_->table_cache()->NewIterator(options, f->number, f->file_size));
        }
    }
}

struct MatchState {
    enum State { kNotFound, kFound, kDeleted, kCorrupt };
    State state{kNotFound};
    std::string value;
    const Comparator* ucmp;
    Slice user_key;
};

static void SaveValue(void* arg, const Slice& ikey, const Slice& v) {
    auto* match = reinterpret_cast<MatchState*>(arg);
    ParsedInternalKey parsed;
    if (!ParseInternalKey(ikey, &parsed)) {
        match->state = MatchState::kCorrupt;
        return;
    }
    if (match->ucmp->Compare(parsed.user_key, match->user_key) == 0) {
        if (parsed.type == kTypeValue) {
            match->state = MatchState::kFound;
            match->value.assign(v.data(), v.size());
        } else if (parsed.type == kTypeDeletion) {
            match->state = MatchState::kDeleted;
        }
    }
}

Status Version::Get(const ReadOptions& options, const LookupKey& k, std::string* value,
                    GetStats* stats, DBStats* db_stats) {
    Slice ikey = k.internal_key();
    Slice user_key = k.user_key();
    const Comparator* ucmp = vset_->icmp().user_comparator();
    Status s;

    MatchState state;
    state.ucmp = ucmp;
    state.user_key = user_key;

    // Search Level 0 in reverse order (newest file to oldest)
    std::vector<FileMetaData*> l0_candidates;
    for (auto it = files_[0].rbegin(); it != files_[0].rend(); ++it) {
        FileMetaData* f = *it;
        if (ucmp->Compare(user_key, ExtractUserKey(f->smallest)) >= 0 &&
            ucmp->Compare(user_key, ExtractUserKey(f->largest)) <= 0) {
            l0_candidates.push_back(f);
        }
    }

    for (FileMetaData* f : l0_candidates) {
        std::shared_ptr<TableReader> table;
        if (vset_->table_cache()->FindTable(f->number, f->file_size, &table).ok()) {
            if (!table->KeyMayMatch(user_key)) {
                if (db_stats) db_stats->bloom_filter_useful.fetch_add(1, std::memory_order_relaxed);
                continue; // Bloom filter eliminated this table!
            }
            if (db_stats) db_stats->bloom_filter_positive.fetch_add(1, std::memory_order_relaxed);
        }

        s = vset_->table_cache()->Get(options, f->number, f->file_size, ikey, &state, &SaveValue);
        if (!s.ok()) return s;

        if (state.state == MatchState::kFound) {
            value->assign(state.value);
            return Status::OK();
        } else if (state.state == MatchState::kDeleted) {
            return Status::NotFound("Deleted");
        } else if (state.state == MatchState::kCorrupt) {
            return Status::Corruption("corrupted key in sstable");
        }
    }

    // Search Levels 1..kNumLevels-1
    for (int level = 1; level < kNumLevels; level++) {
        if (files_[level].empty()) continue;

        int index = FindFile(vset_->icmp(), files_[level], ikey);
        if (index < static_cast<int>(files_[level].size())) {
            FileMetaData* f = files_[level][index];
            if (ucmp->Compare(user_key, ExtractUserKey(f->smallest)) >= 0) {
                std::shared_ptr<TableReader> table;
                if (vset_->table_cache()->FindTable(f->number, f->file_size, &table).ok()) {
                    if (!table->KeyMayMatch(user_key)) {
                        if (db_stats) db_stats->bloom_filter_useful.fetch_add(1, std::memory_order_relaxed);
                        continue;
                    }
                    if (db_stats) db_stats->bloom_filter_positive.fetch_add(1, std::memory_order_relaxed);
                }

                s = vset_->table_cache()->Get(options, f->number, f->file_size, ikey, &state, &SaveValue);
                if (!s.ok()) return s;

                if (state.state == MatchState::kFound) {
                    value->assign(state.value);
                    return Status::OK();
                } else if (state.state == MatchState::kDeleted) {
                    return Status::NotFound("Deleted");
                } else if (state.state == MatchState::kCorrupt) {
                    return Status::Corruption("corrupted key in sstable");
                }
            }
        }
    }

    return Status::NotFound("Key not found");
}

class VersionBuilder {
public:
    VersionBuilder(VersionSet* vset, Version* base)
        : vset_(vset), base_(base) {
        base_->Ref();
        BytewiseComparator();
        for (int level = 0; level < kNumLevels; level++) {
            levels_[level].added_files = std::vector<FileMetaData*>();
        }
    }

    ~VersionBuilder() {
        for (int level = 0; level < kNumLevels; level++) {
            for (auto* f : levels_[level].added_files) {
                if (f->refs <= 0) {
                    delete f;
                }
            }
        }
        base_->Unref();
    }

    void Apply(const VersionEdit* edit) {
        // Delete files
        for (const auto& deleted : edit->deleted_files()) {
            const int level = deleted.first;
            const uint64_t number = deleted.second;
            levels_[level].deleted_files.insert(number);
        }

        // Add files
        for (const auto& new_file : edit->new_files()) {
            const int level = new_file.first;
            FileMetaData* f = new FileMetaData(new_file.second);
            f->refs = 1;
            levels_[level].deleted_files.erase(f->number);
            levels_[level].added_files.push_back(f);
        }
    }

    void SaveTo(Version* v) {
        for (int level = 0; level < kNumLevels; level++) {
            // Keep base files that weren't deleted
            const auto& base_files = base_->files_[level];
            const auto& deleted_files = levels_[level].deleted_files;

            std::vector<FileMetaData*> result;
            result.reserve(base_files.size() + levels_[level].added_files.size());

            for (auto* f : base_files) {
                if (deleted_files.find(f->number) == deleted_files.end()) {
                    f->refs++;
                    result.push_back(f);
                }
            }

            // Append newly added files
            for (auto* f : levels_[level].added_files) {
                f->refs++;
                result.push_back(f);
            }

            // Sort files: L0 sorted by file number ascending; L1..6 sorted by smallest key
            if (level == 0) {
                std::sort(result.begin(), result.end(), [](FileMetaData* a, FileMetaData* b) {
                    return a->number < b->number;
                });
            } else {
                std::sort(result.begin(), result.end(), [this](FileMetaData* a, FileMetaData* b) {
                    return vset_->icmp().Compare(a->smallest, b->smallest) < 0;
                });
            }

            v->files_[level] = std::move(result);
        }
    }

private:
    struct LevelState {
        std::set<uint64_t> deleted_files;
        std::vector<FileMetaData*> added_files;
    };

    VersionSet* vset_;
    Version* base_;
    LevelState levels_[kNumLevels];
};

Compaction::Compaction(const Options* options, int level)
    : level_(level),
      max_output_file_size_(options->max_bytes_for_level_base / 5) {
    if (max_output_file_size_ < 2 * 1024 * 1024) {
        max_output_file_size_ = 2 * 1024 * 1024;
    }
}

Compaction::~Compaction() {
    if (input_version_ != nullptr) {
        input_version_->Unref();
    }
}

bool Compaction::IsTrivialMove() const {
    // Avoid trivial move if there are overlapping files in level+1
    return (num_input_files(0) == 1 &&
            num_input_files(1) == 0 &&
            level_ > 0);
}

void Compaction::AddInputDeletions(VersionEdit* edit) {
    for (int which = 0; which < 2; which++) {
        for (size_t i = 0; i < inputs_[which].size(); i++) {
            edit->DeleteFile(level_ + which, inputs_[which][i]->number);
        }
    }
}

bool Compaction::IsBaseLevelForKey(const Slice& user_key) {
    const Comparator* user_cmp = input_version_->vset_->icmp().user_comparator();
    for (int lvl = level_ + 2; lvl < kNumLevels; lvl++) {
        const auto& files = input_version_->files_[lvl];
        for (const auto* f : files) {
            if (user_cmp->Compare(user_key, ExtractUserKey(f->largest)) <= 0 &&
                user_cmp->Compare(user_key, ExtractUserKey(f->smallest)) >= 0) {
                return false;
            }
        }
    }
    return true;
}

void Compaction::ReleaseInputs() {
    if (input_version_ != nullptr) {
        input_version_->Unref();
        input_version_ = nullptr;
    }
}

VersionSet::VersionSet(std::string dbname, const Options* options,
                       TableCache* table_cache, const InternalKeyComparator* cmp)
    : env_(Env::Default()),
      dbname_(std::move(dbname)),
      options_(options),
      table_cache_(table_cache),
      icmp_(*cmp),
      dummy_versions_(this) {
    AppendVersion(new Version(this));
}

VersionSet::~VersionSet() {
    current_->Unref();
    assert(dummy_versions_.next_ == &dummy_versions_);
}

void VersionSet::AppendVersion(Version* v) {
    assert(v->refs_ == 0);
    assert(v != current_);
    if (current_ != nullptr) {
        current_->Unref();
    }
    current_ = v;
    v->Ref();

    // Append to circular doubly-linked list
    v->prev_ = dummy_versions_.prev_;
    v->next_ = &dummy_versions_;
    v->prev_->next_ = v;
    v->next_->prev_ = v;
}

Status VersionSet::LogAndApply(VersionEdit* edit, std::mutex* mu) {
    if (edit->has_log_number()) {
        assert(edit->log_number() >= log_number_);
        assert(edit->log_number() < next_file_number_);
    } else {
        edit->SetLogNumber(log_number_);
    }

    if (!edit->has_prev_log_number()) {
        edit->SetPrevLogNumber(prev_log_number_);
    }

    edit->SetNextFile(next_file_number_);
    edit->SetLastSequence(last_sequence_);

    Version* v = new Version(this);
    {
        VersionBuilder builder(this, current_);
        builder.Apply(edit);
        builder.SaveTo(v);
    }

    // Initialize new descriptor file if necessary
    std::string new_manifest_file;
    Status s;
    if (descriptor_log_ == nullptr) {
        // No active manifest file, create one
        assert(descriptor_file_ == nullptr);
        new_manifest_file = DescriptorFileName(dbname_, manifest_file_number_);
        s = env_->NewWritableFile(new_manifest_file, &descriptor_file_);
        if (s.ok()) {
            descriptor_log_ = std::make_unique<wal::WalWriter>(descriptor_file_.get());
            // Write snapshot of current state
            VersionEdit snapshot_edit;
            snapshot_edit.SetComparatorName(icmp_.user_comparator()->Name());
            for (int level = 0; level < kNumLevels; level++) {
                for (auto* f : current_->files_[level]) {
                    snapshot_edit.AddFile(level, f->number, f->file_size, f->smallest, f->largest);
                }
            }
            std::string record;
            snapshot_edit.EncodeTo(&record);
            s = descriptor_log_->AddRecord(record);
        }
    }

    // Unlock mutex while writing to manifest
    if (mu) mu->unlock();

    // Write edit to manifest
    if (s.ok()) {
        std::string record;
        edit->EncodeTo(&record);
        s = descriptor_log_->AddRecord(record);
        if (s.ok()) {
            s = descriptor_file_->Sync();
        }
        if (!s.ok()) {
            // Manifest write error
        }
    }

    // Install CURRENT file if manifest changed
    if (s.ok() && !new_manifest_file.empty()) {
        s = SetCurrentFile(env_, dbname_, manifest_file_number_);
    }

    if (mu) mu->lock();

    // Install new version
    if (s.ok()) {
        AppendVersion(v);
        log_number_ = edit->log_number();
        prev_log_number_ = edit->prev_log_number();
    } else {
        delete v;
        if (!new_manifest_file.empty()) {
            descriptor_log_.reset();
            descriptor_file_.reset();
            env_->RemoveFile(new_manifest_file);
        }
    }

    return s;
}

Status VersionSet::Recover() {
    // 1. Read CURRENT file
    std::string current;
    Status s = ReadFileToString(env_, CurrentFileName(dbname_), &current);
    if (!s.ok()) {
        return s;
    }
    if (current.empty() || current.back() != '\n') {
        return Status::Corruption("CURRENT file does not end with newline");
    }
    current.resize(current.size() - 1);

    std::string dscname = dbname_ + "/" + current;
    std::unique_ptr<SequentialFile> file;
    s = env_->NewSequentialFile(dscname, &file);
    if (!s.ok()) {
        return s;
    }

    uint64_t next_file = 0;
    SequenceNumber last_sequence = 0;
    uint64_t log_number = 0;
    uint64_t prev_log_number = 0;
    VersionBuilder builder(this, current_);

    // 2. Replay all VersionEdits in manifest
    wal::WalReader reader(file.get(), nullptr, true /* verify checksums */, 0);
    Slice record;
    std::string scratch;
    while (reader.ReadRecord(&record, &scratch)) {
        VersionEdit edit;
        s = edit.DecodeFrom(record);
        if (!s.ok()) {
            return s;
        }

        if (edit.has_comparator() &&
            edit.comparator() != icmp_.user_comparator()->Name()) {
            return Status::InvalidArgument("manifest comparator does not match: " + edit.comparator());
        }

        builder.Apply(&edit);

        if (edit.has_log_number()) log_number = edit.log_number();
        if (edit.has_prev_log_number()) prev_log_number = edit.prev_log_number();
        if (edit.has_next_file_number()) next_file = edit.next_file_number();
        if (edit.has_last_sequence()) last_sequence = edit.last_sequence();
    }

    if (s.ok()) {
        Version* v = new Version(this);
        builder.SaveTo(v);
        AppendVersion(v);
        manifest_file_number_ = next_file;
        next_file_number_ = next_file + 1;
        last_sequence_ = last_sequence;
        log_number_ = log_number;
        prev_log_number_ = prev_log_number;
    }

    return s;
}

Compaction* VersionSet::PickCompaction() {
    // Determine level with highest compaction score
    compaction_level_ = -1;
    compaction_score_ = -1.0;

    // Check level 0
    double score = static_cast<double>(current_->NumLevelFiles(0)) /
                   static_cast<double>(options_->level0_file_num_compaction_trigger);
    if (score >= 1.0) {
        compaction_level_ = 0;
        compaction_score_ = score;
    }

    // Check higher levels
    for (int lvl = 1; lvl < kNumLevels - 1; lvl++) {
        double s = static_cast<double>(current_->NumLevelBytes(lvl)) /
                   static_cast<double>(MaxBytesForLevel(options_, lvl));
        if (s > compaction_score_) {
            compaction_score_ = s;
            compaction_level_ = lvl;
        }
    }

    if (compaction_score_ < 1.0 || compaction_level_ < 0) {
        return nullptr;
    }

    auto* c = new Compaction(options_, compaction_level_);
    c->input_version_ = current_;
    c->input_version_->Ref();

    // Pick first file in level
    assert(!current_->files_[compaction_level_].empty());
    FileMetaData* f = current_->files_[compaction_level_][0];
    c->inputs_[0].push_back(f);

    SetupOtherInputs(c);
    return c;
}

void VersionSet::SetupOtherInputs(Compaction* c) {
    if (c->inputs_[0].empty()) return;
    const Comparator* ucmp = icmp_.user_comparator();

    if (c->level_ == 0) {
        // Expand L0 inputs iteratively to include all overlapping L0 files
        bool expanded = true;
        while (expanded) {
            expanded = false;
            Slice user_smallest = ExtractUserKey(c->inputs_[0][0]->smallest);
            Slice user_largest = ExtractUserKey(c->inputs_[0][0]->largest);
            for (auto* f : c->inputs_[0]) {
                if (ucmp->Compare(ExtractUserKey(f->smallest), user_smallest) < 0) {
                    user_smallest = ExtractUserKey(f->smallest);
                }
                if (ucmp->Compare(ExtractUserKey(f->largest), user_largest) > 0) {
                    user_largest = ExtractUserKey(f->largest);
                }
            }
            for (FileMetaData* f : current_->files_[0]) {
                if (std::find(c->inputs_[0].begin(), c->inputs_[0].end(), f) != c->inputs_[0].end()) {
                    continue;
                }
                Slice f_small = ExtractUserKey(f->smallest);
                Slice f_large = ExtractUserKey(f->largest);
                if (ucmp->Compare(f_large, user_smallest) >= 0 &&
                    ucmp->Compare(f_small, user_largest) <= 0) {
                    c->inputs_[0].push_back(f);
                    expanded = true;
                }
            }
        }
    }

    // Find all overlapping files in level + 1
    if (c->level_ + 1 < kNumLevels && !c->inputs_[0].empty()) {
        Slice all_user_smallest = ExtractUserKey(c->inputs_[0][0]->smallest);
        Slice all_user_largest = ExtractUserKey(c->inputs_[0][0]->largest);
        for (auto* f : c->inputs_[0]) {
            if (ucmp->Compare(ExtractUserKey(f->smallest), all_user_smallest) < 0) {
                all_user_smallest = ExtractUserKey(f->smallest);
            }
            if (ucmp->Compare(ExtractUserKey(f->largest), all_user_largest) > 0) {
                all_user_largest = ExtractUserKey(f->largest);
            }
        }

        const auto& next_level_files = current_->files_[c->level_ + 1];
        for (auto* f : next_level_files) {
            Slice f_small = ExtractUserKey(f->smallest);
            Slice f_large = ExtractUserKey(f->largest);
            if (ucmp->Compare(f_large, all_user_smallest) >= 0 &&
                ucmp->Compare(f_small, all_user_largest) <= 0) {
                c->inputs_[1].push_back(f);
            }
        }
    }
}

Compaction* VersionSet::CompactRange(int level, const Slice* begin, const Slice* end) {
    if (level >= kNumLevels - 1 || current_->files_[level].empty()) {
        return nullptr;
    }

    std::vector<FileMetaData*> inputs;
    for (auto* f : current_->files_[level]) {
        if (begin != nullptr && icmp_.user_comparator()->Compare(ExtractUserKey(f->largest), *begin) < 0) {
            continue;
        }
        if (end != nullptr && icmp_.user_comparator()->Compare(ExtractUserKey(f->smallest), *end) > 0) {
            continue;
        }
        inputs.push_back(f);
    }

    if (inputs.empty()) return nullptr;

    auto* c = new Compaction(options_, level);
    c->input_version_ = current_;
    c->input_version_->Ref();
    c->inputs_[0] = std::move(inputs);
    SetupOtherInputs(c);
    return c;
}

Iterator* VersionSet::MakeInputIterator(Compaction* c) {
    ReadOptions options;
    options.verify_checksums = options_->paranoid_checks;
    options.fill_cache = false;

    // Build iterators for all inputs in c->inputs_[0] and c->inputs_[1]
    const size_t num_iters = c->inputs_[0].size() + c->inputs_[1].size();
    auto* list = new Iterator*[num_iters];
    size_t count = 0;

    for (int which = 0; which < 2; which++) {
        for (auto* f : c->inputs_[which]) {
            list[count++] = table_cache_->NewIterator(options, f->number, f->file_size);
        }
    }

    Iterator* result = NewMergingIterator(&icmp_, list, static_cast<int>(count));
    delete[] list;
    return result;
}

void VersionSet::AddLiveFiles(std::set<uint64_t>* live) {
    for (Version* v = dummy_versions_.next_; v != &dummy_versions_; v = v->next_) {
        for (int level = 0; level < kNumLevels; level++) {
            for (const auto* f : v->files_[level]) {
                live->insert(f->number);
            }
        }
    }
}

} // namespace lsmforge
