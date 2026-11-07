module;
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

module cm:state_file_impl;

import std;
import cm.core;
import :state_file;

namespace cm {

void write_atomically(const std::filesystem::path& file, const std::string_view content)
{
    auto temporary = file;
    temporary += ".tmp";

    const int descriptor = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (descriptor < 0) {
        throw std::system_error{errno, std::generic_category(), "open"};
    }

    struct Closer
    {
        int descriptor;

        ~Closer()
        {
            ::close(descriptor);
        }
    } closer{descriptor};

    auto rest = content;
    while (not rest.empty()) {
        const auto written = ::write(descriptor, rest.data(), rest.size());
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::system_error{errno, std::generic_category(), "write"};
        }
        rest.remove_prefix(static_cast<std::size_t>(written));
    }
    if (::fsync(descriptor) != 0) {
        throw std::system_error{errno, std::generic_category(), "fsync"};
    }

    std::filesystem::rename(temporary, file);

    // The rename only lasts through a power loss once the directory has reached the disk as well.
    const auto directory = file.has_parent_path() ? file.parent_path() : std::filesystem::path{"."};
    const int directory_descriptor = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory_descriptor < 0) {
        throw std::system_error{errno, std::generic_category(), "open directory"};
    }
    const Closer directory_closer{directory_descriptor};
    if (::fsync(directory_descriptor) != 0) {
        throw std::system_error{errno, std::generic_category(), "fsync directory"};
    }
}

std::optional<std::string> read_state_file(const std::filesystem::path& file, const log::Logger& logger)
{
    std::error_code ec;
    if (not std::filesystem::exists(file, ec) || ec) {
        SPDLOG_LOGGER_DEBUG(logger, "Nothing stored at '{}' yet.", file.string());
        return std::nullopt;
    }

    std::ifstream stream{file, std::ios::binary};
    if (not stream.is_open()) {
        SPDLOG_LOGGER_ERROR(logger, "Failed to open '{}'.", file.string());
        return std::nullopt;
    }
    // A read that breaks off leaves the content short, and parsing it then fails with the file's name.
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

StateFileWriter::StateFileWriter(std::filesystem::path file)
    : file_{std::move(file)}
{
}

StateFileWriter::~StateFileWriter()
{
    writer_.join();
}

void StateFileWriter::save(std::string content)
{
    // Only the newest state is worth a write, as the writer can lag behind on a slow SD card.
    const auto generation = ++generation_;
    boost::asio::post(writer_, [this, generation, content = std::move(content)]() {
        if (generation != generation_) {
            return;
        }
        try {
            write_atomically(file_, content);
        }
        catch (const std::exception& error) {
            SPDLOG_LOGGER_ERROR(logger_, "Could not save '{}': {}", file_.string(), error.what());
        }
    });
}

void StateFileWriter::flush()
{
    std::promise<void> done;
    boost::asio::post(writer_, [&done]() { done.set_value(); });
    done.get_future().wait();
}

const std::filesystem::path& StateFileWriter::file() const
{
    return file_;
}
} // namespace cm
