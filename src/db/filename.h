#pragma once

#include "lsmforge/slice.h"
#include "lsmforge/status.h"
#include "util/env.h"
#include <cstdint>
#include <cstdio>
#include <string>

namespace lsmforge {

enum FileType {
    kLogFile,
    kDBLockFile,
    kTableFile,
    kDescriptorFile,
    kCurrentFile,
    kTempFile,
    kInfoLogFile
};

// Return the name of the log file with the specified number
// in the db named "dbname".
inline std::string LogFileName(const std::string& dbname, uint64_t number) {
    char buf[100];
    std::snprintf(buf, sizeof(buf), "%s/wal/%06llu.wal", dbname.c_str(),
                  static_cast<unsigned long long>(number));
    return buf;
}

// Return the name of the sstable with the specified number
// in the db named "dbname".
inline std::string TableFileName(const std::string& dbname, uint64_t number) {
    char buf[100];
    std::snprintf(buf, sizeof(buf), "%s/sst/%06llu.sst", dbname.c_str(),
                  static_cast<unsigned long long>(number));
    return buf;
}

// Return the name of the descriptor (manifest) file with the specified
// pattern in the db named "dbname".
inline std::string DescriptorFileName(const std::string& dbname, uint64_t number) {
    char buf[100];
    std::snprintf(buf, sizeof(buf), "%s/MANIFEST-%06llu", dbname.c_str(),
                  static_cast<unsigned long long>(number));
    return buf;
}

// Return the name of the CURRENT file in the db named "dbname".
inline std::string CurrentFileName(const std::string& dbname) {
    return dbname + "/CURRENT";
}

// Return the name of the lock file in the db named "dbname".
inline std::string LockFileName(const std::string& dbname) {
    return dbname + "/LOCK";
}

// Return the name of a temporary file with the specified number
// in the db named "dbname".
inline std::string TempFileName(const std::string& dbname, uint64_t number) {
    char buf[100];
    std::snprintf(buf, sizeof(buf), "%s/tmp/%06llu.tmp", dbname.c_str(),
                  static_cast<unsigned long long>(number));
    return buf;
}

// Set *number to the file number and *type to the file type
// of the given file name.
bool ParseFileName(const std::string& filename, uint64_t* number, FileType* type);

// Atomically writes the CURRENT file pointing to the manifest file "desc_name"
Status SetCurrentFile(Env* env, const std::string& dbname, uint64_t descriptor_number);

} // namespace lsmforge
