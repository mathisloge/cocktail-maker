module;
#include <boost/asio/any_io_executor.hpp>

export module cm:fill_level;

import std;
import mp_units;
import cm.core;
import :pod_types;
import :dispenser;
import :station_config;
import :pod_registry;
import :recipe;
import :state_file;

namespace cm {

/// A bottle isn't filled to the last drop, and load cells drift, so a warning leaves this much in reserve.
export inline constexpr units::Litre kFillLevelReserve = 20.0 * units::milli_litre;

/// The Pod treats every liquid as 1 g per mL, so the Station has to as well.
export inline constexpr units::GramPerLitre kAssumedDensity = 1000.0 * units::gram_per_litre;

export [[nodiscard]] units::Litre to_volume(units::Grams mass);

/// What the Station knows about the bottle of one dispenser.
export struct FillLevelRecord
{
    /// What is left in the bottle. It can go below 0 when the record has drifted from the real bottle.
    units::Grams level;
    /// The level at the last refill, which is 100 %.
    std::optional<units::Grams> capacity;
    /// The lowest level since the last refill. A refill's rise is measured against it, because any read between filling the
    /// bottle and confirming the refill has already moved the level.
    units::Grams low_water;
    /// The last pour ended `bottle_empty` and no refill has confirmed it yet. A blocked hose looks the same.
    bool suspected_empty{};

    friend bool operator==(const FillLevelRecord&, const FillLevelRecord&) = default;
};

/// What the UI shows about the bottle of one dispenser.
export struct FillLevelView
{
    /// Never below 0. Empty means unknown.
    std::optional<units::Grams> level;
    std::optional<units::Percent> percent;
    bool suspected_empty{};
    /// The Pod says this dispenser has no load cell, which is normal for some dispensers.
    bool no_load_cell{};
};

/// Who says that a bottle has been refilled.
export enum class RefillSource
{
    /// The dialog in a mix, which a guest can confirm too.
    mix_dialog,
    /// The admin area, which is behind the operator PIN.
    admin_area,
};

export struct FillLevelOptions
{
    /// A rise below this isn't a refill, so that hours of drift don't count as one.
    units::Grams refill_min_rise{50 * units::si::gram};
    /// Hands may still be on the bottle when the button is pressed.
    std::chrono::milliseconds refill_settle_time{std::chrono::seconds{1}};
};

/**
 * @brief Keeps the fill level of every dispenser, which the Station owns (ADR-007).
 *
 * A load cell only says how far the weight has moved since its zero point, and that point moves at every power up. So the
 * level always is `reading + correction`, and the correction is worked out again when a Pod connects.
 *
 * All calls have to run on the execution context, apart from `init` and `flush`. Reads that fail never fail the call: there
 * is just no new value then, and a dispenser without a load cell stays quiet this way. Saving happens on a thread of its
 * own, so nothing here blocks the execution context.
 */
export class FillLevelTracker
{
  public:
    using ChangeHandler = std::function<void(const PodDispenser&, const FillLevelView&)>;

    FillLevelTracker(boost::asio::any_io_executor executor, std::filesystem::path file, FillLevelOptions options = {});
    FillLevelTracker(const FillLevelTracker&) = delete;
    FillLevelTracker& operator=(const FillLevelTracker&) = delete;

    /// Loads the stored records. A missing or malformed file logs and starts empty, and a malformed entry is skipped.
    void init();

    /// Blocks until everything saved so far has reached the file, so it must not run on the execution context.
    void flush();

    void set_change_handler(ChangeHandler handler);

    [[nodiscard]] std::optional<FillLevelRecord> record(const PodDispenser& address) const;
    [[nodiscard]] FillLevelView view(const PodDispenser& address) const;

    /// Works out the correction for this dispenser, as its load cell may have lost its zero point. It reads every dispenser
    /// once, so that one without a load cell shows as such.
    Task<void> correct_on_connect(const PodDispenser& address, Dispenser& dispenser);

    /// Forgets the corrections of a Pod, as nobody knows what happens to its load cells until it connects again.
    void pod_disconnected(const PodId& pod_id);

    /// Reads the weight and updates the level.
    Task<void> refresh(const PodDispenser& address, Dispenser& dispenser);

    /// Reads the weight after a pour, and falls back to the reported amount if that fails.
    Task<void> pour_finished(const PodDispenser& address, Dispenser& dispenser, const DispenseReport& report);

    /// A tare of the empty scale moved the zero point while the bottle was off it, so the level is unknown now.
    void clear_scale_tared(const PodDispenser& address);

    /// A tare with the empty bottle on the scale, so the level is 0 and the reading from now on is the liquid alone.
    void empty_bottle_tared(const PodDispenser& address);

    /// Waits for the load to settle and takes what the scale then shows as the new 100 %, if it really is a refill.
    Task<void> refilled(const PodDispenser& address, Dispenser& dispenser, RefillSource source);

  private:
    using Records = std::map<PodDispenser, FillLevelRecord>;

    Task<std::optional<units::Grams>> read_weight(const PodDispenser& address, Dispenser& dispenser);
    void apply_reading(const PodDispenser& address, units::Grams reading);
    void changed(const PodDispenser& address);

    log::Logger logger_{log::create_or_get("fill_level")};
    boost::asio::any_io_executor executor_;
    FillLevelOptions options_;
    Records records_;
    std::map<PodDispenser, units::Grams> corrections_;
    std::set<PodDispenser> without_load_cell_;
    ChangeHandler change_handler_;
    StateFileWriter writer_;
};

/// An ingredient whose bottle may not be enough for a drink.
export struct FillShortage
{
    IngredientId ingredient;
    units::Litre available;
    units::Litre needed;
};

/// The assigned dispensers that `commands` pour from, each one once.
export [[nodiscard]] std::vector<PodDispenser> involved_dispensers(const Commands& commands, const StationConfig& config);

/// The ingredients that may not be enough. An unknown level or a missing assignment gives no shortage.
export [[nodiscard]] std::vector<FillShortage> find_fill_shortages(const Commands& commands,
                                                                   const StationConfig& config,
                                                                   const FillLevelTracker& fill_levels);

/// Brings the levels of a Pod that has just connected up to date, reading one dispenser after the other.
export Task<void> sync_pod_fill_levels(PodInfo info, const PodRegistry& registry, FillLevelTracker& fill_levels);
} // namespace cm
