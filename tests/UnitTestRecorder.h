#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

struct TestCaseRecord {
    std::string id;
    std::string description;
    std::string expected_result;
    std::string pass_fail;
    std::string what_went_wrong;
    std::string trial_date;
    std::string owner;
};

/// Records unit-test assertions, prints progress, and appends `test_cases.csv`.
class UnitTestRecorder {
public:
    explicit UnitTestRecorder(std::string default_owner = "Saleh Yahya / Christopher Fawaz");

    /// Records one assertion; prints `[PASS]` / `[FAIL]`. Returns whether the condition held.
    bool record(bool condition_met,
                 std::string id,
                 std::string_view description,
                 std::string_view expected_result,
                 std::string_view owner,
                 std::string_view condition_text);

    void print_summary(std::ostream& out = std::cout) const;

    /// Appends rows; writes the CSV header only if the file is missing or empty.
    void save_csv(const std::string& path) const;

    int passed() const noexcept { return passed_; }
    int failed() const noexcept { return failed_; }

private:
    static std::string make_timestamp();
    static std::string resolve_owner(std::string_view owner, const std::string& default_owner);

    std::vector<TestCaseRecord> records_;
    std::string default_owner_;
    int passed_ = 0;
    int failed_ = 0;
};
