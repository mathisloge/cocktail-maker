module;

export module cm:ingredient;
import std;

export namespace cm {
/**
 * @brief Categorizes how an ingredient should respond to a boost adjustment.
 *
 * Used by the boost feature to determine how an ingredient's quantity
 * changes when a recipe is positively or negatively boosted.
 */
enum class BoostCategory
{
    /**
     * @brief Quantity remains unchanged regardless of boost.
     *
     * Example: Fixed garnishes such as mint leaves or a slice of lemon.
     */
    fixed,
    /**
     * @brief Quantity increases when boost is positive, decreases when boost is negative.
     *
     * Example: Alcoholic ingredients such as rum or vodka.
     */
    boostable,
    /**
     * @brief Quantity decreases when boost is positive, increases when boost is negative.
     *
     * Example: Non-alcoholic ingredients such as juice or cola.
     */
    reducible,
};

/**
 * @brief Categorizes the broad type of an ingredient.
 *
 * Used to classify ingredients by their nature, which determines
 * how they are handled in recipes and displayed in the UI.
 */
enum class IngredientType
{
    /**
     * @brief Alcoholic spirits.
     *
     * Example: Rum, vodka, gin, tequila.
     */
    alcohol,

    /**
     * @brief Freshly squeezed or pressed fruit and vegetable juices.
     *
     * Example: Lime juice, orange juice, maracuja juice.
     */
    juice,

    /**
     * @brief Sweet, viscous liquids used for flavoring and color.
     *
     * Example: Grenadine, simple syrup, orgeat.
     */
    syrup,

    /**
     * @brief Carbonated mixers.
     *
     * Example: Cola, tonic water, ginger beer.
     */
    soda,

    /**
     * @brief Still or sparkling water without added flavoring.
     *
     * Example: Still water, sparkling water.
     */
    water,

    /**
     * @brief Dairy-based ingredients.
     *
     * Example: Cream, milk, coconut cream.
     */
    dairy,

    /**
     * @brief Concentrated flavor extracts used in small quantities.
     *
     * Example: Angostura bitters, Peychaud's bitters.
     */
    bitters,

    /**
     * @brief Thick blended fruit or vegetable bases.
     *
     * Example: Mango puree, passion fruit puree.
     */
    puree,

    /**
     * @brief Any ingredient that does not fit the categories above.
     */
    other,
};

using IngredientId = std::string;

struct Ingredient
{
    IngredientId id;
    std::string display_name;
    IngredientType type;
    BoostCategory boost_category{};
};
} // namespace cm