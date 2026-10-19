#include "table/bloom_filter.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace lsmforge {

TEST(BloomFilterTest, EmptyFilter) {
    BloomFilterPolicy policy(10);
    std::string filter;
    policy.CreateFilter(nullptr, 0, &filter);
    EXPECT_FALSE(policy.KeyMayMatch("hello", filter));
}

TEST(BloomFilterTest, ZeroFalseNegatives) {
    BloomFilterPolicy policy(10);
    std::vector<std::string> key_strings;
    std::vector<Slice> keys;

    const int n = 5000;
    for (int i = 0; i < n; ++i) {
        key_strings.push_back("key_" + std::to_string(i));
    }
    for (int i = 0; i < n; ++i) {
        keys.push_back(key_strings[i]);
    }

    std::string filter;
    policy.CreateFilter(keys.data(), n, &filter);

    // CRITICAL: Bloom filters must NEVER produce false negatives
    for (int i = 0; i < n; ++i) {
        EXPECT_TRUE(policy.KeyMayMatch(keys[i], filter))
            << "False negative encountered for key: " << keys[i].ToString();
    }
}

TEST(BloomFilterTest, EmpiricalFalsePositiveRate) {
    // With 10 bits per key, expected false positive rate is ~1% (between 0.5% and 2.0%)
    BloomFilterPolicy policy(10);
    std::vector<std::string> inserted_keys;
    std::vector<Slice> keys;

    const int n = 10000;
    inserted_keys.reserve(n);
    keys.reserve(n);
    for (int i = 0; i < n; ++i) {
        inserted_keys.push_back("inserted_key_" + std::to_string(i));
    }
    for (int i = 0; i < n; ++i) {
        keys.emplace_back(inserted_keys[i]);
    }

    std::string filter;
    policy.CreateFilter(keys.data(), n, &filter);

    // Test with disjoint keys
    int false_positives = 0;
    const int test_n = 10000;
    for (int i = 0; i < test_n; ++i) {
        std::string test_key = "non_existent_key_" + std::to_string(i);
        if (policy.KeyMayMatch(test_key, filter)) {
            false_positives++;
        }
    }

    double fp_rate = static_cast<double>(false_positives) / test_n;
    // Expected ~ 1% false positives; ensure it is strictly below 2.5%
    EXPECT_LT(fp_rate, 0.025);
    EXPECT_GT(fp_rate, 0.001);
}

} // namespace lsmforge
