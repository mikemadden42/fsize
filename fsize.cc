#include <cstdint>
#include <filesystem>
#include <future>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

std::uintmax_t calculate_directory_size(const fs::path& path) {
    std::uintmax_t size = 0;
    std::error_code ec;
    fs::recursive_directory_iterator it(
        path, fs::directory_options::skip_permission_denied, ec);
    for (const fs::recursive_directory_iterator end; !ec && it != end;
         it.increment(ec)) {
        std::error_code entry_ec;
        if (it->is_symlink(entry_ec) || entry_ec) {
            continue;
        }
        if (it->is_regular_file(entry_ec)) {
            auto file_size = it->file_size(entry_ec);
            if (!entry_ec) {
                size += file_size;
            }
        }
    }
    if (ec) {
        std::cerr << "warning: " << path.string() << ": " << ec.message()
                  << " (size may be incomplete)\n";
    }
    return size;
}

int main() {
    std::vector<std::future<std::pair<std::string, std::uintmax_t>>> futures;
    std::error_code ec;
    for (fs::directory_iterator it(".", ec), end; !ec && it != end;
         it.increment(ec)) {
        const auto& entry = *it;
        std::error_code entry_ec;
        if (entry.is_symlink(entry_ec) || entry_ec) {
            continue;
        }
        if (entry.is_directory(entry_ec)) {
            futures.push_back(std::async(std::launch::async, [=] {
                return std::make_pair(entry.path().string(),
                                      calculate_directory_size(entry.path()));
            }));
        }
    }
    if (ec) {
        std::cerr << "error: .: " << ec.message() << "\n";
    }

    int status = ec ? 1 : 0;
    for (auto& future : futures) {
        try {
            auto [name, size] = future.get();
            std::cout << name << ": " << size << " bytes\n";
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            status = 1;
        }
    }

    return status;
}
