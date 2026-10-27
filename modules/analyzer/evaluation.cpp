#include "question_analyzer.h"
#include "cli_support.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace {
std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> result;
    std::istringstream input(line);
    std::string field;
    while (std::getline(input, field, '\t')) result.push_back(field);
    return result;
}

std::string oneLine(std::string value) {
    for (auto& c : value) if (c == '\t' || c == '\r' || c == '\n') c = ' ';
    return value;
}
}

int main(int argc, char** argv) {
    try {
        const auto args = analyzer_cli::arguments(argc, argv);
        if (args.size() < 2 || args.size() > 3)
            throw std::runtime_error("用法: analyzer_evaluation 冻结题集.tsv [实际结果.tsv]");
        std::ifstream input(std::filesystem::u8path(args[1]), std::ios::binary);
        if (!input) throw std::runtime_error("无法读取冻结题集");
        std::ofstream report;
        if (args.size() == 3) {
            report.open(std::filesystem::u8path(args[2]), std::ios::binary | std::ios::trunc);
            if (!report) throw std::runtime_error("无法写入实际结果");
            report << "id\tquestion\texpected_task\tactual_task\texpected_uncertain\tactual_uncertain\tpassed\treason\n";
        }
        std::string line;
        std::getline(input, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line != "id\tgroup\texpected_task\texpected_uncertain\tevidence\tquestion")
            throw std::runtime_error("冻结题集表头无效");
        std::set<std::string> ids;
        std::map<std::string, int> groups;
        int count = 0, passed = 0;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            const auto row = fields(line);
            if (row.size() != 6 || !ids.insert(row[0]).second || row[4].empty() || row[5].empty() ||
                !gateway_contract::isTaskName(row[2]) || (row[3] != "true" && row[3] != "false"))
                throw std::runtime_error("冻结题集包含无效行或重复编号");
            ++groups[row[1]];
            const auto actual = gateway_contract::analyzeQuestion(row[5]);
            bool stable = true;
            for (int repeat = 0; repeat < 3; ++repeat) {
                const auto again = gateway_contract::analyzeQuestion(row[5]);
                stable = stable && again.task == actual.task && again.reason == actual.reason &&
                    again.uncertain == actual.uncertain;
            }
            const bool ok = actual.task == row[2] && actual.uncertain == (row[3] == "true") &&
                actual.reason.find(row[4]) != std::string::npos && stable &&
                (!actual.uncertain || actual.task == "general");
            ++count;
            if (ok) ++passed;
            std::cout << (ok ? "[PASS] " : "[FAIL] ") << row[0] << ' ' << actual.task
                      << " uncertain=" << (actual.uncertain ? "true" : "false") << '\n';
            if (!ok) std::cout << actual.reason << '\n';
            if (report.is_open()) report << row[0] << '\t' << row[5] << '\t' << row[2] << '\t' << actual.task
                << '\t' << row[3] << '\t' << (actual.uncertain ? "true" : "false") << '\t'
                << (ok ? "true" : "false") << '\t' << oneLine(actual.reason) << '\n';
        }
        const std::map<std::string, int> expected_groups{
            {"programming", 5}, {"math", 5}, {"translation", 5}, {"writing", 5}, {"general", 5}, {"mixed", 5}};
        if (count != 30 || groups != expected_groups) throw std::runtime_error("冻结题集必须是五类各五条，加五条混合或模糊问题");
        if (report.is_open()) {
            report.flush();
            if (!report) throw std::runtime_error("实际结果写入失败");
        }
        std::cout << "Frozen evaluation: " << passed << "/" << count << " passed.\n";
        return passed == count ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << error.what() << '\n';
        return 1;
    }
}
