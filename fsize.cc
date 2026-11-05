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

int main(int argc, char* argv[]) {
    if (argc > 2) {
        std::cerr << "usage: fsize [directory]\n";
        return 1;
    }
    const fs::path root = argc == 2 ? argv[1] : ".";

    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        std::cerr << "error: " << root.string() << ": "
                  << (ec ? ec.message() : "not a directory") << "\n";
        return 1;
    }

    std::vector<std::future<std::pair<std::string, std::uintmax_t>>> futures;
    std::uintmax_t total = 0;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end;
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
        } else if (entry.is_regular_file(entry_ec)) {
            auto file_size = entry.file_size(entry_ec);
            if (!entry_ec) {
                total += file_size;
            }
        }
    }
    if (ec) {
        std::cerr << "error: " << root.string() << ": " << ec.message() << "\n";
    }

    int status = ec ? 1 : 0;
    for (auto& future : futures) {
        try {
            auto [name, size] = future.get();
            std::cout << name << ": " << size << " bytes\n";
            total += size;
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            status = 1;
        }
    }
    std::cout << "total: " << total << " bytes\n";

    return status;
}
