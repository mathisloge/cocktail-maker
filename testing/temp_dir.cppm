module;

export module cm.testing:temp_dir;
import std;

namespace cm::testing {
/// A directory of its own for one test. It is removed with everything in it when the object goes away.
export class TempDir
{
  public:
    explicit TempDir(std::string_view prefix = "cm-test")
        : path_{unique_path(prefix)}
    {
        std::filesystem::create_directories(path_);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;

    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    [[nodiscard]] const std::filesystem::path& path() const
    {
        return path_;
    }

    /// The path of the file `name` in this directory. The file isn't created.
    [[nodiscard]] std::filesystem::path file(std::string_view name) const
    {
        return path_ / name;
    }

    /// Writes `content` to the file `name`, replacing it.
    /// @throws std::runtime_error if the file can't be written.
    void write_file(std::string_view name, std::string_view content) const
    {
        const auto target = file(name);
        std::ofstream stream{target, std::ios::binary | std::ios::trunc};
        stream << content;
        if (not stream) {
            throw std::runtime_error{std::format("Could not write '{}'", target.string())};
        }
    }

  private:
    static std::filesystem::path unique_path(std::string_view prefix)
    {
        static std::atomic<std::uint64_t> counter{0};
        const auto stamp = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        return std::filesystem::temp_directory_path() / std::format("{}-{}-{}", prefix, stamp, counter++);
    }

    std::filesystem::path path_;
};
} // namespace cm::testing
