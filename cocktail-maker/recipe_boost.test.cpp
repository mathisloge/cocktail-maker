#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
import std;
import cm;
import cm.core;
import mp_units;

using namespace cm;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

// ─── Fixtures ─────────────────────────────────────────────────────────────────

constexpr auto kEps = std::numeric_limits<double>::epsilon();
constexpr auto kBiggerEps = 1e-9;

auto make_store() -> IngredientStore
{
    IngredientStore store;
    store.init_ingredients({
        {IngredientId{"rum"}, "Dark Rum", IngredientType::alcohol, BoostCategory::boostable},
        {IngredientId{"vodka"}, "Vodka", IngredientType::alcohol, BoostCategory::boostable},
        {IngredientId{"cola"}, "Cola", IngredientType::soda, BoostCategory::reducible},
        {IngredientId{"juice"}, "Orange Juice", IngredientType::juice, BoostCategory::reducible},
        {IngredientId{"mint"}, "Mint Leaves", IngredientType::other, BoostCategory::fixed},
    });
    return store;
}

// ─── Command builders ─────────────────────────────────────────────────────────

auto dispense(std::string id, double ml) -> Command
{
    return DispenseCommand{CommandId{0}, IngredientId{std::move(id)}, ml * units::milli_litre};
}

auto manual(std::string instruction) -> Command
{
    return ManualCommand{CommandId{0}, std::move(instruction)};
}

// ─── Inspection helpers ───────────────────────────────────────────────────────

// Returns the volume of the first DispenseCommand matching `id`,
// searching both sequential and parallel slots.
auto find_volume(const Commands& commands, std::string id_str) -> std::optional<units::Litre>
{
    const IngredientId id{std::move(id_str)};
    const auto check_cmd = [&](const Command& cmd) -> std::optional<units::Litre> {
        if (const auto* dc = std::get_if<DispenseCommand>(&cmd); dc && dc->ingredient == id) {
            return dc->volume;
        }
        return std::nullopt;
    };

    for (const auto& item : commands) {
        if (auto found = std::visit(
                [&](const auto& v) -> std::optional<units::Litre> {
                    using V = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<V, Command>) {
                        return check_cmd(v);
                    }
                    else {
                        for (const auto& cmd : v) {
                            if (auto vol = check_cmd(cmd)) {
                                return vol;
                            }
                        }
                        return std::nullopt;
                    }
                },
                item)) {
            return found;
        }
    }
    return std::nullopt;
}

// Sums the volumes of all DispenseCommands in mL.
auto total_volume(const Commands& commands) -> units::Litre
{
    units::Litre total = 0.0 * units::milli_litre;
    const auto add_cmd = [&](const Command& cmd) {
        if (const auto* dc = std::get_if<DispenseCommand>(&cmd)) {
            total += dc->volume;
        }
    };
    for (const auto& item : commands) {
        std::visit(
            [&](const auto& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<V, Command>) {
                    add_cmd(v);
                }
                else {
                    std::ranges::for_each(v, add_cmd);
                }
            },
            item);
    }
    return total;
}

} // namespace

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("boost: zero boost returns commands unchanged", "[boost]")
{
    const auto store = make_store();
    const Commands commands = {dispense("rum", 40.0), dispense("cola", 60.0)};

    const auto result = boost_recipe(commands, 0.0 * units::percent, store);

    CHECK_THAT(find_volume(result, "rum")->numerical_value_in(units::milli_litre), WithinAbs(40., kEps));
    CHECK_THAT(find_volume(result, "cola")->numerical_value_in(units::milli_litre), WithinAbs(60.0, kEps));
}

TEST_CASE("boost: positive boost shifts volume from reducible to boostable", "[boost]")
{
    const auto store = make_store();
    // V_b=40, V_r=60, p=0.5
    // boostable_scale = 1 + 0.5*(60/40) = 1.75  →  rum  = 40*1.75 = 70 ml
    // reducible_scale = 1 - 0.5          = 0.5   →  cola = 60*0.5  = 30 ml
    const Commands commands = {dispense("rum", 40.0), dispense("cola", 60.0)};

    const auto result = boost_recipe(commands, 50.0 * units::percent, store);

    CHECK_THAT(find_volume(result, "rum")->numerical_value_in(units::milli_litre), WithinAbs(70.0, kEps));
    CHECK_THAT(find_volume(result, "cola")->numerical_value_in(units::milli_litre), WithinAbs(30.0, kEps));
}

TEST_CASE("boost: +100% drains reducible completely into boostable", "[boost]")
{
    const auto store = make_store();
    const Commands commands = {dispense("rum", 40.0), dispense("cola", 60.0)};

    const auto result = boost_recipe(commands, 100.0 * units::percent, store);

    CHECK_THAT(find_volume(result, "rum")->numerical_value_in(units::milli_litre), WithinAbs(100.0, kEps));
    CHECK_THAT(find_volume(result, "cola")->numerical_value_in(units::milli_litre), WithinAbs(0.0, kEps));
}

TEST_CASE("boost: negative boost shifts volume from boostable to reducible", "[boost]")
{
    const auto store = make_store();
    // V_b=40, V_r=60, p=-0.5
    // boostable_scale = 1 + (-0.5)           = 0.5   →  rum  = 40*0.5 = 20 ml
    // reducible_scale = 1 - (-0.5)*(40/60)   = 4/3   →  cola = 60*(4/3) = 80 ml
    const Commands commands = {dispense("rum", 40.0), dispense("cola", 60.0)};

    const auto result = boost_recipe(commands, -50.0 * units::percent, store);

    CHECK_THAT(find_volume(result, "rum")->numerical_value_in(units::milli_litre), WithinAbs(20.0, kEps));
    CHECK_THAT(find_volume(result, "cola")->numerical_value_in(units::milli_litre), WithinAbs(80.0, kBiggerEps));
}

TEST_CASE("boost: -100% drains boostable completely into reducible", "[boost]")
{
    const auto store = make_store();
    const Commands commands = {dispense("rum", 40.0), dispense("cola", 60.0)};

    const auto result = boost_recipe(commands, -100.0 * units::percent, store);

    CHECK_THAT(find_volume(result, "rum")->numerical_value_in(units::milli_litre), WithinAbs(0.0, kEps));
    CHECK_THAT(find_volume(result, "cola")->numerical_value_in(units::milli_litre), WithinAbs(100.0, kBiggerEps));
}

TEST_CASE("boost: fixed ingredients are never scaled", "[boost]")
{
    const auto store = make_store();
    // mint is fixed — must survive any boost level unchanged.
    // 40ml rum + 5ml mint + 55ml cola = 100ml
    const Commands commands = {
        dispense("rum", 40.0),
        dispense("mint", 5.0),
        dispense("cola", 55.0),
    };

    SECTION("+50 %")
    {
        const auto result = boost_recipe(commands, 50.0 * units::percent, store);
        CHECK_THAT(find_volume(result, "mint")->numerical_value_in(units::milli_litre), WithinAbs(5.0, kEps));
    }
    SECTION("-50 %")
    {
        const auto result = boost_recipe(commands, -50.0 * units::percent, store);
        CHECK_THAT(find_volume(result, "mint")->numerical_value_in(units::milli_litre), WithinAbs(5.0, kEps));
    }
    SECTION("+100 %")
    {
        const auto result = boost_recipe(commands, 100.0 * units::percent, store);
        CHECK_THAT(find_volume(result, "mint")->numerical_value_in(units::milli_litre), WithinAbs(5.0, kEps));
    }
    SECTION("-100 %")
    {
        const auto result = boost_recipe(commands, -100.0 * units::percent, store);
        CHECK_THAT(find_volume(result, "mint")->numerical_value_in(units::milli_litre), WithinAbs(5.0, kEps));
    }
}

TEST_CASE("boost: multiple boostable ingredients all scale by the same factor", "[boost]")
{
    const auto store = make_store();
    // V_b=50, V_r=50, p=0.5
    // boostable_scale = 1 + 0.5*(50/50) = 1.5
    // reducible_scale = 0.5
    const Commands commands = {
        dispense("rum", 30.0),
        dispense("vodka", 20.0),
        dispense("cola", 50.0),
    };

    const auto result = boost_recipe(commands, 50.0 * units::percent, store);

    CHECK_THAT(find_volume(result, "rum")->numerical_value_in(units::milli_litre), WithinAbs(45.0, kEps));
    CHECK_THAT(find_volume(result, "vodka")->numerical_value_in(units::milli_litre), WithinAbs(30.0, kEps));
    CHECK_THAT(find_volume(result, "cola")->numerical_value_in(units::milli_litre), WithinAbs(25.0, kEps));
}

TEST_CASE("boost: multiple reducible ingredients all scale by the same factor", "[boost]")
{
    const auto store = make_store();
    // V_b=50, V_r=50, p=-0.5
    // boostable_scale = 0.5
    // reducible_scale = 1 - (-0.5)*(50/50) = 1.5
    const Commands commands = {
        dispense("rum", 50.0),
        dispense("cola", 30.0),
        dispense("juice", 20.0),
    };

    const auto result = boost_recipe(commands, -50.0 * units::percent, store);

    CHECK_THAT(find_volume(result, "rum")->numerical_value_in(units::milli_litre), WithinAbs(25.0, kEps));
    CHECK_THAT(find_volume(result, "cola")->numerical_value_in(units::milli_litre), WithinAbs(45.0, kEps));
    CHECK_THAT(find_volume(result, "juice")->numerical_value_in(units::milli_litre), WithinAbs(30.0, kEps));
}

TEST_CASE("boost: total dispensed volume is conserved for all boost levels", "[boost]")
{
    const auto store = make_store();
    // Fixed ingredients count toward total volume and must not change it either.
    const Commands commands = {
        dispense("rum", 40.0),
        dispense("mint", 5.0),
        dispense("cola", 55.0),
    };
    const auto original_total = total_volume(commands).numerical_value_in(units::milli_litre); // 100 ml

    const double p = GENERATE(-100.0, -75.0, -50.0, -25.0, 0.0, 25.0, 50.0, 75.0, 100.0);
    INFO("boost = " << p << " %");

    const auto result = boost_recipe(commands, p * units::percent, store);
    CHECK_THAT(total_volume(result).numerical_value_in(units::milli_litre), WithinRel(original_total, kEps));
}

TEST_CASE("boost: DispenseCommands inside a ParallelCommand are scaled", "[boost]")
{
    const auto store = make_store();
    // Same math as the basic +50% test, just delivered in parallel.
    const Commands commands = {
        ParallelCommand{dispense("rum", 40.0), dispense("cola", 60.0)},
    };

    const auto result = boost_recipe(commands, 50.0 * units::percent, store);

    CHECK_THAT(find_volume(result, "rum")->numerical_value_in(units::milli_litre), WithinAbs(70.0, kEps));
    CHECK_THAT(find_volume(result, "cola")->numerical_value_in(units::milli_litre), WithinAbs(30.0, kEps));
}

TEST_CASE("boost: ManualCommand content passes through unchanged", "[boost]")
{
    const auto store = make_store();
    const std::string instruction = "Stir vigorously for 10 seconds";
    const Commands commands = {
        manual(instruction),
        dispense("rum", 40.0),
        dispense("cola", 60.0),
    };

    const auto result = boost_recipe(commands, 50.0 * units::percent, store);

    REQUIRE(result.size() == 3);
    const auto* cmd = std::get_if<Command>(result.data());
    REQUIRE(cmd != nullptr);
    const auto* mc = std::get_if<ManualCommand>(cmd);
    REQUIRE(mc != nullptr);
    CHECK(mc->instruction == instruction);
}

TEST_CASE("boost: DispenseCommand for unknown ingredient passes through unchanged", "[boost]")
{
    const auto store = make_store();
    // "mystery" is not registered in the store — should be left as-is rather than dropped.
    const Commands commands = {
        dispense("mystery", 30.0),
        dispense("rum", 30.0),
        dispense("cola", 40.0),
    };

    const auto result = boost_recipe(commands, 50.0 * units::percent, store);

    CHECK_THAT(find_volume(result, "mystery")->numerical_value_in(units::milli_litre), WithinAbs(30.0, kEps));
    CHECK_THAT(total_volume(result).numerical_value_in(units::milli_litre),
               WithinRel(total_volume(commands).numerical_value_in(units::milli_litre), kEps));
}

TEST_CASE("boost: no boostable ingredients — positive boost is a no-op", "[boost]")
{
    // Regression test for the volume-drain bug: with V_b=0 the original code set
    // reducible_scale = 1−p but boostable_scale = 1.0, silently losing volume.
    const auto store = make_store();
    const Commands commands = {
        dispense("cola", 60.0),
        dispense("juice", 40.0),
        dispense("mint", 5.0),
    };

    const auto result = boost_recipe(commands, 50.0 * units::percent, store);

    CHECK_THAT(find_volume(result, "cola")->numerical_value_in(units::milli_litre), WithinAbs(60.0, kEps));
    CHECK_THAT(find_volume(result, "juice")->numerical_value_in(units::milli_litre), WithinAbs(40.0, kEps));
    CHECK_THAT(find_volume(result, "mint")->numerical_value_in(units::milli_litre), WithinAbs(5.0, kEps));
    CHECK_THAT(total_volume(result).numerical_value_in(units::milli_litre),
               WithinRel(total_volume(commands).numerical_value_in(units::milli_litre), kEps));
}

TEST_CASE("boost: no reducible ingredients — negative boost is a no-op", "[boost]")
{
    // Symmetric regression: with V_r=0 the original code divided by zero
    // (guarded) but still shrank boostable_scale = 1+p, losing volume.
    const auto store = make_store();
    const Commands commands = {
        dispense("rum", 40.0),
        dispense("vodka", 60.0),
        dispense("mint", 5.0),
    };

    const auto result = boost_recipe(commands, -50.0 * units::percent, store);

    CHECK_THAT(find_volume(result, "rum")->numerical_value_in(units::milli_litre), WithinAbs(40.0, kEps));
    CHECK_THAT(find_volume(result, "vodka")->numerical_value_in(units::milli_litre), WithinAbs(60.0, kEps));
    CHECK_THAT(find_volume(result, "mint")->numerical_value_in(units::milli_litre), WithinAbs(5.0, kEps));
    CHECK_THAT(total_volume(result).numerical_value_in(units::milli_litre),
               WithinRel(total_volume(commands).numerical_value_in(units::milli_litre), kEps));
}
