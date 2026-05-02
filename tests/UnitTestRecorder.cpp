#include "UnitTestRecorder.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>

UnitTestRecorder::UnitTestRecorder(std::string default_owner)
    : default_owner_(std::move(default_owner)) {}

std::string UnitTestRecorder::make_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

std::string UnitTestRecorder::resolve_owner(std::string_view owner, const std::string& default_owner) {
    if (owner.empty())
        return default_owner;
    return std::string(owner);
}

bool UnitTestRecorder::record(bool condition_met,
                              std::string id,
                              std::string_view description,
                              std::string_view expected_result,
                              std::string_view owner,
                              std::string_view condition_text) {
    std::cout << (condition_met ? "  [PASS] " : "  [FAIL] ") << description << '\n';
    if (condition_met)
        ++passed_;
    else
        ++failed_;

    records_.push_back(TestCaseRecord{
        std::move(id),
        std::string(description),
        std::string(expected_result),
        condition_met ? "PASS" : "FAIL",
        condition_met ? std::string() : std::string("Condition not met: ") + std::string(condition_text),
        make_timestamp(),
        resolve_owner(owner, default_owner_),
    });
    return condition_met;
}

void UnitTestRecorder::print_summary(std::ostream& out) const {
    out << "\n--- Results: " << passed_ << " passed, " << failed_ << " failed ---\n";
}

void UnitTestRecorder::save_csv(const std::string& path) const {
    if (records_.empty()) {
        std::cout << "No test records to save for this run.\n";
        return;
    }

    std::ifstream check(path);
    const bool file_missing_or_empty =
        !check.good() || check.peek() == std::ifstream::traits_type::eof();
    check.close();

    std::ofstream out(path, std::ios::app);
    if (!out) {
        std::cerr << "Failed to open for append: " << path << '\n';
        return;
    }

    if (file_missing_or_empty) {
        out << "id,description,expected_result,pass_fail,what_went_wrong,trial_date,owner\n";
    }

    for (const auto& r : records_) {
        out << '"' << r.id << "\","
            << '"' << r.description << "\","
            << '"' << r.expected_result << "\","
            << '"' << r.pass_fail << "\","
            << '"' << r.what_went_wrong << "\","
            << '"' << r.trial_date << "\","
            << '"' << r.owner << "\"\n";
    }

    std::cout << "Test results written to: " << path << " (" << records_.size() << " records)\n";
}
