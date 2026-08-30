module;
export module cm:station_config;

import std;
import cm.core;
import :pod_types;
import :ingredient;
import :dispenser;

namespace cm {

/**
 * @brief Addresses a single dispenser on a specific pod.
 *
 * Used as the value side of the ingredient assignment kept by StationConfig. The pair is treated as an opaque address:
 * neither the pod nor the dispenser has to be discovered or connected for a PodDispenser to be valid.
 */
export struct PodDispenser
{
    PodId pod_id;
    DispenserId dispenser_id;

    friend constexpr auto operator<=>(const PodDispenser&, const PodDispenser&) = default;
};

/**
 * @brief Reported when a lookup finds no dispenser assigned to an ingredient.
 *
 * Carries the ingredient that was looked up so a caller can report or retry without re-deriving it.
 */
export class IngredientNotAssignedError : public std::runtime_error
{
  public:
    explicit IngredientNotAssignedError(IngredientId ingredient_id)
        : runtime_error{std::format("No dispenser is assigned to ingredient '{}'", ingredient_id)}
        , ingredient_id_{std::move(ingredient_id)}
    {
    }

    /**
     * @brief The ingredient that has no dispenser assigned.
     */
    IngredientId ingredient_id() const
    {
        return ingredient_id_;
    }

  private:
    IngredientId ingredient_id_;
};

/**
 * @brief Persistent assignment of ingredients to pod dispensers.
 *
 * Holds the station's answer to "which dispenser pours this ingredient", and writes every change straight back to a JSON
 * file so the assignment survives a restart.
 *
 * Every operation runs synchronously on the calling thread and performs file I/O.
 */
export class StationConfig final
{
  public:
    /**
     * @brief Binds the configuration to the ingredient catalog it validates against.
     *
     * @param ingredient_store Catalog used to reject assignments for unknown ingredients. Held by reference and must
     * outlive this object.
     * @param db_file Path of the configuration database.
     *
     * @note `db_file` is currently stored but never read. The mapping is always saved to and loaded from
     * `ingredient_dispenser_mapping.json` in the working directory.
     */
    explicit StationConfig(const IngredientStore& ingredient_store, std::filesystem::path db_file);

    /**
     * @brief Loads the stored assignment, replacing whatever is held in memory.
     *
     * A missing, unreadable or malformed file is not an error. It is logged and leaves the station with an empty
     * assignment. Individual entries that are malformed, or that name an ingredient the ingredient store does not know,
     * are skipped while the rest is kept.
     *
     * @post The in-memory assignment reflects the file, or is empty if the file could not be read.
     */
    void init();

    /**
     * @brief Assigns an ingredient to a dispenser and persists the change immediately.
     *
     * An existing assignment for the same ingredient is replaced. The pod and dispenser are deliberately not validated
     * against discovered hardware, because the configuration is loaded before any pod is connected.
     *
     * @param ingredient_id Must be known to the ingredient store, otherwise the call is logged and ignored.
     */
    void update_dispenser_ingredient_mapping(IngredientId ingredient_id, PodDispenser pod_dispenser_pair);

    /**
     * @brief Looks up the dispenser assigned to an ingredient.
     *
     * @returns The assigned dispenser, or IngredientNotAssignedError carrying `ingredient_id` if the ingredient has no
     * assignment.
     */
    std::expected<PodDispenser, IngredientNotAssignedError> find_dispenser_for_ingredient(IngredientId ingredient_id) const;

    /**
     * @brief Looks up which ingredient a dispenser is assigned to.
     *
     * The reverse direction is a linear scan over all assignments, unlike the hashed forward lookup.
     *
     * @returns The assigned ingredient, or std::out_of_range if the pod/dispenser pair carries no ingredient.
     */
    std::expected<IngredientId, std::out_of_range> find_ingredient_by_dispenser(PodDispenser pod_dispenser) const;

  private:
    /**
     * @brief Resolves the location of the mapping file.
     *
     * @returns `ingredient_dispenser_mapping.json` in the current working directory, or the bare relative name if the
     * working directory cannot be determined.
     */
    static std::filesystem::path mapping_file_path();

    /**
     * @brief Writes the whole assignment to the mapping file, truncating it.
     *
     * Serialization and write failures are logged and swallowed, so a caller cannot tell an assignment was lost.
     */
    void save_config_to_file() const;

    /**
     * @brief Replaces the in-memory assignment with the contents of the mapping file.
     *
     * @pre The ingredient store is populated. Entries naming an unknown ingredient are dropped.
     */
    void load_config_from_file();

  private:
    log::Logger logger_{log::create_or_get("station_config")};
    const IngredientStore& ingredient_store_;
    const std::filesystem::path db_file_;
    std::unordered_map<IngredientId, PodDispenser> ingredient_dispenser_mapping_;
};
} // namespace cm
