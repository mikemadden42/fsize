#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

std::uintmax_t calculate_directory_size(const fs::path& path) {
    std::uintmax_t size = 0;
    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        if (entry.is_regular_file()) {
            size += entry.file_size();
        }
    }
    return size;
}

int main() {
    std::vector<std::future<std::pair<std::string, std::uintmax_t>>> futures;
    for (const auto& entry : fs::directory_iterator(".")) {
        if (entry.is_directory()) {
            futures.push_back(std::async(std::launch::async, [=] {
                return std::make_pair(entry.path().string(),
                                      calculate_directory_size(entry.path()));
            }));
        }
    }

    for (auto& future : futures) {
        auto [name, size] = future.get();
        std::cout << name << ": " << size << " bytes\n";
    }

    return 0;
}
