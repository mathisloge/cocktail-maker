// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>
#include <cm/glass_store.hpp>

using namespace cm;

constexpr auto kStandardGlass = "StandardGlass";
constexpr auto kSmallGlass = "SmallGlass";
constexpr auto kLargeGlass = "LargeGlass";

class GlassStoreFixture
{
  public:
    GlassStoreFixture()
    {
        store.add_glass(
            Glass{
                .id = kStandardGlass,
                .display_name = kStandardGlass,
                .image_path = "",
                .capacity = 0.25 * units::si::litre,
            },
            300 * units::si::gram);

        store.add_glass(
            Glass{
                .id = kSmallGlass,
                .display_name = kSmallGlass,
                .image_path = "",
                .capacity = 0.1 * units::si::litre,
            },
            100 * units::si::gram);

        store.add_glass(
            Glass{
                .id = kLargeGlass,
                .display_name = kLargeGlass,
                .image_path = "",
                .capacity = 0.3 * units::si::litre,
            },
            350 * units::si::gram);
    }

  protected:
    cm::GlassStore store; // NOLINT
};

TEST_CASE_METHOD(GlassStoreFixture, "a glass can be found by weight", "[GlassStore]")
{
    SECTION("small glass")
    {
        REQUIRE(store.find_glass_by_weight(100 * units::si::gram).display_name == kSmallGlass);
    }
    SECTION("standard glass")
    {
        REQUIRE(store.find_glass_by_weight(300 * units::si::gram).display_name == kStandardGlass);
    }
    SECTION("large glass")
    {
        REQUIRE(store.find_glass_by_weight(350 * units::si::gram).display_name == kLargeGlass);
    }
    SECTION("within tolerance")
    {
        REQUIRE(store.find_glass_by_weight(295 * units::si::gram).display_name == kStandardGlass);
        REQUIRE(store.find_glass_by_weight(305 * units::si::gram).display_name == kStandardGlass);
        REQUIRE_THROWS(store.find_glass_by_weight(294 * units::si::gram));
        REQUIRE_THROWS(store.find_glass_by_weight(306 * units::si::gram));
    }
    SECTION("find best match")
    {
        constexpr auto kNearSmallGlass = "NearSmall";
        store.add_glass(
            Glass{
                .display_name = kNearSmallGlass,
                .image_path = "",
                .capacity = 0.1 * units::si::litre,
            },
            103 * units::si::gram);

        REQUIRE(store.find_glass_by_weight(100 * units::si::gram).display_name == kSmallGlass);
        REQUIRE(store.find_glass_by_weight(101 * units::si::gram).display_name == kSmallGlass);
        REQUIRE(store.find_glass_by_weight(102 * units::si::gram).display_name == kNearSmallGlass);
        REQUIRE(store.find_glass_by_weight(103 * units::si::gram).display_name == kNearSmallGlass);
    }
}
