#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <exception>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// Sizes a set of top-level folders with a fixed pool of worker threads.
// Each job lists a single folder and queues its subfolders as new jobs, so
// all workers can share the walk of one large folder.
class SizeScanner {
   public:
    explicit SizeScanner(const std::vector<fs::path>& folders)
        : sizes_(folders.size()) {
        for (std::size_t i = 0; i < folders.size(); ++i) {
            queue_.push_back({folders[i], i});
        }
        pending_ = queue_.size();
    }

    void run(unsigned thread_count) {
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < thread_count; ++i) {
            threads.emplace_back([this] { work(); });
        }
        for (auto& thread : threads) {
            thread.join();
        }
    }

    std::uintmax_t size(std::size_t folder) const { return sizes_[folder]; }
    bool failed() const { return failed_; }

   private:
    struct Job {
        fs::path path;
        std::size_t folder;  // index of the top-level folder it counts toward
    };

    void work() {
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock,
                         [this] { return !queue_.empty() || pending_ == 0; });
                if (queue_.empty()) {
                    return;
                }
                job = std::move(queue_.front());
                queue_.pop_front();
            }

            try {
                scan(job);
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(mutex_);
                std::cerr << "error: " << job.path.string() << ": " << e.what()
                          << "\n";
                failed_ = true;
            }

            std::lock_guard<std::mutex> lock(mutex_);
            if (--pending_ == 0) {
                cv_.notify_all();
            }
        }
    }

    // Lists one folder, adds its regular files to the job's top-level folder,
    // and queues its subfolders.
    void scan(const Job& job) {
        std::uintmax_t size = 0;
        std::vector<fs::path> subfolders;
        std::error_code ec;
        for (fs::directory_iterator it(
                 job.path, fs::directory_options::skip_permission_denied, ec),
             end;
             !ec && it != end; it.increment(ec)) {
            std::error_code entry_ec;
            if (it->is_symlink(entry_ec) || entry_ec) {
                continue;
            }
            if (it->is_directory(entry_ec)) {
                subfolders.push_back(it->path());
            } else if (it->is_regular_file(entry_ec)) {
                auto file_size = it->file_size(entry_ec);
                if (!entry_ec) {
                    size += file_size;
                }
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (ec) {
            std::cerr << "warning: " << job.path.string() << ": "
                      << ec.message() << " (size may be incomplete)\n";
        }
        sizes_[job.folder] += size;
        for (auto& subfolder : subfolders) {
            queue_.push_back({std::move(subfolder), job.folder});
        }
        pending_ += subfolders.size();
        cv_.notify_all();
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Job> queue_;
    std::size_t pending_ = 0;  // jobs queued or being scanned
    std::vector<std::uintmax_t> sizes_;
    bool failed_ = false;
};

// Formats a size as bytes, or with -h in powers of 1024 like "1.5M".
std::string format_size(std::uintmax_t bytes, bool human) {
    if (!human) {
        return std::to_string(bytes) + " bytes";
    }
    if (bytes < 1024) {
        return std::to_string(bytes) + "B";
    }
    const char units[] = "KMGTPE";
    double size = bytes / 1024.0;
    std::size_t unit = 0;
    // Move up a unit when one decimal place would round to 1024.0.
    while (size >= 1023.95 && unit + 1 < sizeof(units) - 1) {
        size /= 1024;
        ++unit;
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f%c", size, units[unit]);
    return buffer;
}

int usage() {
    std::cerr << "usage: fsize [-h] [directory]\n";
    return 1;
}

int main(int argc, char* argv[]) {
    bool human = false;
    fs::path root;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h") {
            human = true;
        } else if (arg.size() > 1 && arg[0] == '-') {
            return usage();
        } else if (root.empty()) {
            root = arg;
        } else {
            return usage();
        }
    }
    if (root.empty()) {
        root = ".";
    }

    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        std::cerr << "error: " << root.string() << ": "
                  << (ec ? ec.message() : "not a directory") << "\n";
        return 1;
    }

    std::vector<fs::path> folders;
    std::uintmax_t total = 0;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end;
         it.increment(ec)) {
        const auto& entry = *it;
        std::error_code entry_ec;
        if (entry.is_symlink(entry_ec) || entry_ec) {
            continue;
        }
        if (entry.is_directory(entry_ec)) {
            folders.push_back(entry.path());
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

    SizeScanner scanner(folders);
    scanner.run(std::max(1u, std::thread::hardware_concurrency()));

    // Largest first, with ties in name order.
    std::vector<std::pair<std::uintmax_t, std::string>> results;
    for (std::size_t i = 0; i < folders.size(); ++i) {
        results.emplace_back(scanner.size(i), folders[i].string());
        total += scanner.size(i);
    }
    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });

    for (const auto& [size, name] : results) {
        std::cout << name << ": " << format_size(size, human) << "\n";
    }
    std::cout << "total: " << format_size(total, human) << "\n";

    return ec || scanner.failed() ? 1 : 0;
}
