module;
#include <boost/asio/thread_pool.hpp>

export module cm:state_file;

import std;
import cm.core;

namespace cm {
/**
 * Replaces `file` in one step, so that a power loss leaves either the old content or the new one.
 *
 * @throws std::system_error or std::filesystem::filesystem_error if a step fails.
 */
export void write_atomically(const std::filesystem::path& file, std::string_view content);

/**
 * Reads a file that holds Station state.
 *
 * @returns The content, or nothing for a file that doesn't exist or can't be read. The first only logs at debug level,
 * as a fresh Station has no state yet, and the second logs an error.
 */
export [[nodiscard]] std::optional<std::string> read_state_file(const std::filesystem::path& file, const log::Logger& logger);

/**
 * Saves Station state on a thread of its own, so that a slow SD card never blocks the caller.
 *
 * Only the newest content counts. A save that still waits for the writer when a newer one arrives is skipped, and a
 * write that fails is logged.
 */
export class StateFileWriter
{
  public:
    explicit StateFileWriter(std::filesystem::path file);
    StateFileWriter(const StateFileWriter&) = delete;
    StateFileWriter& operator=(const StateFileWriter&) = delete;
    StateFileWriter(StateFileWriter&&) = delete;
    StateFileWriter& operator=(StateFileWriter&&) = delete;

    /// Waits until the last write has finished.
    ~StateFileWriter();

    void save(std::string content);

    /// Blocks until everything saved so far has reached the file, so it must not run on the execution context.
    void flush();

    [[nodiscard]] const std::filesystem::path& file() const;

  private:
    log::Logger logger_{log::create_or_get("state_file")};
    std::filesystem::path file_;
    std::atomic<std::uint64_t> generation_{0};
    boost::asio::thread_pool writer_{1};
};
} // namespace cm
