#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
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

    for (std::size_t i = 0; i < folders.size(); ++i) {
        std::cout << folders[i].string() << ": " << scanner.size(i)
                  << " bytes\n";
        total += scanner.size(i);
    }
    std::cout << "total: " << total << " bytes\n";

    return ec || scanner.failed() ? 1 : 0;
}
