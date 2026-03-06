#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <string>
#include <vector>
#include <sstream>
#include <cmath>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

namespace fs = std::filesystem;

// Get target directory from environment variable MANDEYE_OUTPUT_DIR, else use PWD or getcwd
fs::path get_target_dir() {
    if (const char* env = std::getenv("MANDEYE_OUTPUT_DIR")) {
        return fs::path(env);
    }
    if (const char* pwd = std::getenv("PWD")) {
        return fs::path(pwd);
    }
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf)) != nullptr) {
        return fs::path(buf);
    }
    return fs::current_path();
}

// Find next filename with pattern prefix_<n>.<ext> (n is integer). If none found start at 0.
fs::path next_filename(const fs::path& dir, const std::string& prefix, const std::string& ext) {
    std::regex re("^" + prefix + "_(\\\\d+)\\." + ext + "$");
    unsigned long max_index = 0;
    bool found = false;
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        throw std::runtime_error("Directory does not exist: " + dir.string());
    }
    for (auto &entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::smatch m;
        std::string name = entry.path().filename().string();
        if (std::regex_match(name, m, re) && m.size() >= 2) {
            try {
                unsigned long idx = std::stoul(m[1].str());
                if (!found || idx > max_index) max_index = idx;
                found = true;
            } catch (...) { }
        }
    }
    unsigned long next = found ? (max_index + 1) : 0;
    std::ostringstream oss;
    oss << prefix << "_" << next << "." << ext;
    return dir / oss.str();
}

// Write file using O_DIRECT if possible, fall back to normal write. Returns seconds elapsed.
// fileSizeMB: size in MB to write (1 MB = 1024*1024 bytes)
double write_benchmark(const fs::path& path, size_t fileSizeMB, bool try_direct=true) {
    const size_t bufSize = 1024 * 1024; // 1 MB
    double elapsedSec = 0.0;
    // Prepare buffer for non-direct write
    std::vector<char> buffer(bufSize, 0xAA);

    if (try_direct) {
        // Try O_DIRECT path
        void* aligned = nullptr;
        const size_t alignment = 4096;
        if (posix_memalign(&aligned, alignment, bufSize) == 0) {
            memset(aligned, 0xAA, bufSize);
            int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0666);
            if (fd >= 0) {
                auto start = std::chrono::high_resolution_clock::now();
                bool ok = true;
                for (size_t i = 0; i < fileSizeMB; ++i) {
                    ssize_t w = write(fd, aligned, bufSize);
                    if (w != (ssize_t)bufSize) { ok = false; break; }
                }
                fsync(fd);
                close(fd);
                auto end = std::chrono::high_resolution_clock::now();
                if (ok) {
                    elapsedSec = std::chrono::duration<double>(end - start).count();
                    free(aligned);
                    return elapsedSec; // success with direct
                }
            }
            // fallback path if direct fails
            if (aligned) free(aligned);
        }
    }

    // Normal buffered write
    {
        std::ofstream out(path, std::ios::binary);
        if (!out) throw std::runtime_error("Failed to open file for writing: " + path.string());
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < fileSizeMB; ++i) {
            out.write(buffer.data(), bufSize);
            if (!out) throw std::runtime_error("Write failed to " + path.string());
        }
        out.close();
        // ensure data is pushed to disk
        int fd = open(path.c_str(), O_RDONLY);
        if (fd >= 0) {
            fsync(fd);
            close(fd);
        }
        auto end = std::chrono::high_resolution_clock::now();
        elapsedSec = std::chrono::duration<double>(end - start).count();
    }
    return elapsedSec;
}

int main(int argc, char** argv) {
    // Usage: benchmark_append [sizeMB] [count] [prefix] [ext]
    size_t sizeMB = 10;
    size_t count = 1;
    std::string prefix = "bench";
    std::string ext = "bin";

    if (argc >= 2) sizeMB = std::stoul(argv[1]);
    if (argc >= 3) count = std::stoul(argv[2]);
    if (argc >= 4) prefix = argv[3];
    if (argc >= 5) ext = argv[4];

    fs::path dir = get_target_dir();
    if (!fs::exists(dir)) {
        std::cerr << "Target directory does not exist: " << dir << '\n';
        return 2;
    }

    std::cout << "Target dir: " << dir << '\n';
    std::cout << "Writing " << count << " files of " << sizeMB << " MB each with prefix '" << prefix << "' ext '" << ext << "'\n";

    struct Result { std::string file; double seconds; double mbps; };
    std::vector<Result> results;

    for (size_t i = 0; i < count; ++i) {
        fs::path file = next_filename(dir, prefix, ext);
        try {
            double sec = write_benchmark(file, sizeMB, true);
            double mbps = (double)sizeMB / (sec > 0 ? sec : 1e-9);
            // Round to 2 decimal places for nicer JSON-like output
            mbps = std::round(mbps * 100.0) / 100.0;
            results.push_back({file.filename().string(), sec, mbps});
            std::cout << "Wrote: " << file.filename() << ", " << sizeMB << " MB in " << sec << " s (" << mbps << " MB/s)\n";
        } catch (const std::exception& ex) {
            std::cerr << "Error writing file " << file << ": " << ex.what() << '\n';
            return 3;
        }
    }

    // Print JSON summary (simple)
    std::cout << "[\n";
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "  { \"file\": \"" << results[i].file << "\", \"mbps\": " << std::fixed << std::setprecision(2) << results[i].mbps << " }";
        if (i + 1 < results.size()) std::cout << ',';
        std::cout << "\n";
    }
    std::cout << "]\n";

    return 0;
}

