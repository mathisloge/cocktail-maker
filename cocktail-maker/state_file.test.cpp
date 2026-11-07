#include <catch2/catch_test_macros.hpp>
import std;
import cm.core;
import cm.testing;
import cm;

namespace {
std::string read_all(const std::filesystem::path& file)
{
    std::ifstream stream{file, std::ios::binary};
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}
} // namespace

TEST_CASE("StateFileWriter writes the newest content it was given", "[state_file]")
{
    cm::testing::TempDir dir;
    cm::StateFileWriter writer{dir.file("state.json")};

    writer.save("first");
    writer.save("second");
    writer.flush();

    CHECK(read_all(dir.file("state.json")) == "second");
    CHECK_FALSE(std::filesystem::exists(dir.file("state.json.tmp")));
}

TEST_CASE("StateFileWriter finishes its last write when it goes away", "[state_file]")
{
    cm::testing::TempDir dir;
    {
        cm::StateFileWriter writer{dir.file("state.json")};
        writer.save("last");
    }

    CHECK(read_all(dir.file("state.json")) == "last");
}

TEST_CASE("read_state_file gives nothing for a file that doesn't exist", "[state_file]")
{
    cm::testing::TempDir dir;

    CHECK_FALSE(cm::read_state_file(dir.file("missing.json"), cm::log::create_or_get("test")).has_value());
}

TEST_CASE("read_state_file reads the whole file", "[state_file]")
{
    cm::testing::TempDir dir;
    dir.write_file("state.json", R"({"version": 1})");

    CHECK(cm::read_state_file(dir.file("state.json"), cm::log::create_or_get("test")) == R"({"version": 1})");
}
