#define CATCH_CONFIG_MAIN
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

import std;
import cm;

struct MeterTag
{
};

using Meter = cm::
    strong_type<double, MeterTag, cm::Comparable, cm::Additive, cm::Subtractive, cm::Arithmetic, cm::Formattable, cm::Hashable>;

using Second = cm::strong_type<double, struct SecondTag, cm::Comparable, cm::Additive, cm::Subtractive>;

using Id = cm::strong_type<std::uint32_t, struct IdTag, cm::Comparable, cm::Hashable>;

using Counter = cm::strong_type<int, struct CounterTag, cm::Comparable, cm::Incrementable, cm::Decrementable>;

using SaturatingInt = cm::strong_type<int, struct SaturatingTag, cm::Comparable, cm::SaturatingArithmetic>;

template <class L, class R>
concept plusable = requires(L l, R r) { l + r; };

template <class L, class R>
concept comparable_to = requires(L l, R r) { l <=> r; };

static_assert(std::is_same_v<Meter::underlying_type, double>);
static_assert(std::is_same_v<Meter::tag_type, MeterTag>);
static_assert(sizeof(Meter) == sizeof(double));
static_assert(std::is_default_constructible_v<Meter>);
static_assert(std::is_constructible_v<Meter, double>);
static_assert(!std::is_convertible_v<double, Meter>);
static_assert(!std::is_convertible_v<Meter, double>);
static_assert(!plusable<Meter, Second>);
static_assert(!comparable_to<Meter, Second>);
static_assert(!std::same_as<Meter, Second>);
static_assert(std::three_way_comparable<Meter>);
static_assert(std::equality_comparable<Meter>);

TEST_CASE("strong_type preserves raw value access and explicit conversion", "[strong_type]")
{
    Meter m{12.5};

    REQUIRE(m.raw() == Catch::Approx(12.5));
    REQUIRE(static_cast<const double&>(m) == Catch::Approx(12.5));

    auto explicit_conv = static_cast<double>(Meter{3.25});
    REQUIRE(explicit_conv == Catch::Approx(3.25));
}

TEST_CASE("strong_type comparison works only inside the same strong type", "[strong_type]")
{
    Meter a{1.0};
    Meter b{2.0};
    Meter c{2.0};

    REQUIRE(a < b);
    REQUIRE(b > a);
    REQUIRE(b == c);
    REQUIRE((a <=> b) < 0);

    Second s{2.0};
    REQUIRE_FALSE(plusable<Meter, Second>);
    REQUIRE_FALSE(comparable_to<Meter, Second>);
    (void)s;
}

TEST_CASE("strong_type additive and subtractive behavior is type-safe", "[strong_type]")
{
    Meter a{5.0};
    Meter b{2.5};

    REQUIRE((a + b).raw() == Catch::Approx(7.5));
    REQUIRE((a - b).raw() == Catch::Approx(2.5));

    a += b;
    REQUIRE(a.raw() == Catch::Approx(7.5));
    a -= Meter{1.5};
    REQUIRE(a.raw() == Catch::Approx(6.0));
}

TEST_CASE("strong_type increment and decrement work for discrete domains", "[strong_type]")
{
    Counter c{7};

    REQUIRE(++c == Counter{8});
    REQUIRE(c++ == Counter{8});
    REQUIRE(c == Counter{9});

    REQUIRE(--c == Counter{8});
    REQUIRE(c-- == Counter{8});
    REQUIRE(c == Counter{7});
}

TEST_CASE("strong_type arithmetic skill exposes scalar multiplication and division", "[strong_type]")
{
    Meter m{3.0};

    REQUIRE((m * 2.0).raw() == Catch::Approx(6.0));
    REQUIRE((2.0 * m).raw() == Catch::Approx(6.0));
    REQUIRE((m / 2.0).raw() == Catch::Approx(1.5));

    m *= 4.0;
    REQUIRE(m.raw() == Catch::Approx(12.0));
    m /= 3.0;
    REQUIRE(m.raw() == Catch::Approx(4.0));
}

TEST_CASE("strong_type hashable work in unordered containers", "[strong_type]")
{
    std::unordered_set<Meter> values;
    values.insert(Meter{1.0});
    values.insert(Meter{2.0});
    values.insert(Meter{1.0});

    REQUIRE(values.size() == 2);
    REQUIRE(values.contains(Meter{1.0}));
    REQUIRE(values.contains(Meter{2.0}));
}

TEST_CASE("strong_type formattable format through std::format", "[strong_type]")
{
    Meter m{12.5};
    REQUIRE(std::format("{}", m) == std::format("{}", 12.5));
}

TEST_CASE("strong_type saturating arithmetic clamps at the numeric bounds", "[strong_type]")
{
    SaturatingInt hi{std::numeric_limits<int>::max()};
    SaturatingInt one{1};

    auto saturated = saturating_add(hi, one);
    REQUIRE(saturated.raw() == std::numeric_limits<int>::max());

    SaturatingInt lo{std::numeric_limits<int>::min()};
    auto saturated_sub = saturating_sub(lo, one);
    REQUIRE(saturated_sub.raw() == std::numeric_limits<int>::min());
}

TEST_CASE("strong_type different tags stay incompatible even when the underlying type matches", "[strong_type]")
{
    Meter m{1.0};
    Second s{1.0};

    REQUIRE_FALSE(plusable<Meter, Second>);
    REQUIRE_FALSE(std::is_assignable_v<Meter&, Second>);
    REQUIRE(m.raw() == Catch::Approx(1.0));
    REQUIRE(s.raw() == Catch::Approx(1.0));
}
