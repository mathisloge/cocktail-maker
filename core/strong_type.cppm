module;
export module cm.core:strong_type;
import std;

namespace cm {

// -----------------------------------------------------------------------------
// Skill markers
// -----------------------------------------------------------------------------

// clang-format off
export struct Comparable {};
export struct Additive {};
export struct Subtractive {};
export struct Incrementable {};
export struct Decrementable {};
export struct Hashable {};
export struct Formattable {};
export struct Arithmetic {};
export struct SaturatingArithmetic {};

// clang-format on

namespace detail {

template <class T, class... Ts>
inline constexpr bool contains_v = (std::same_as<T, Ts> || ...);

template <class T, class... Ts>
consteval bool occurs_once()
{
    return ((std::same_as<T, Ts> ? 1u : 0u) + ...) == 1u;
}

template <class... Ts>
consteval bool unique()
{
    return (occurs_once<Ts, Ts...>() && ...);
}

template <class... Ts>
inline constexpr bool unique_v = unique<Ts...>();

template <class... Skills>
consteval bool has_conflicts()
{
    constexpr bool additive = contains_v<Additive, Skills...>;
    constexpr bool subtractive = contains_v<Subtractive, Skills...>;
    constexpr bool saturating = contains_v<SaturatingArithmetic, Skills...>;

    // Saturating arithmetic is intentionally exclusive with regular additive/
    // subtractive operators to keep the operator contract deterministic.
    return saturating && (additive || subtractive);
}

template <class T>
constexpr T saturating_add(T lhs, T rhs) noexcept
{
    static_assert(std::is_integral_v<T>, "saturating arithmetic is only enabled for integral underlying types");

    using limits = std::numeric_limits<T>;

    if constexpr (std::is_signed_v<T>) {
        if ((rhs > 0) && (lhs > limits::max() - rhs)) {
            return limits::max();
        }
        if ((rhs < 0) && (lhs < limits::min() - rhs)) {
            return limits::min();
        }
    }
    else {
        if (lhs > limits::max() - rhs) {
            return limits::max();
        }
    }

    return static_cast<T>(lhs + rhs);
}

template <class T>
constexpr T saturating_sub(T lhs, T rhs) noexcept
{
    static_assert(std::is_integral_v<T>, "saturating arithmetic is only enabled for integral underlying types");

    using limits = std::numeric_limits<T>;

    if constexpr (std::is_signed_v<T>) {
        if ((rhs < 0) && (lhs > limits::max() + rhs)) {
            return limits::max();
        }
        if ((rhs > 0) && (lhs < limits::min() + rhs)) {
            return limits::min();
        }
    }
    else {
        if (lhs < rhs) {
            return 0;
        }
    }

    return static_cast<T>(lhs - rhs);
}

} // namespace detail

export template <class T, class Tag, class... Skills>
    requires std::is_object_v<T> && std::is_class_v<Tag> && detail::unique_v<Skills...>
class strong_type
{
    static_assert(!detail::has_conflicts<Skills...>(), "SaturatingArithmetic is exclusive with Additive/Subtractive");

  public:
    using underlying_type = T;
    using tag_type = Tag;

    static constexpr bool has_comparable = detail::contains_v<Comparable, Skills...>;
    static constexpr bool has_additive = detail::contains_v<Additive, Skills...>;
    static constexpr bool has_subtractive = detail::contains_v<Subtractive, Skills...>;
    static constexpr bool has_incrementable = detail::contains_v<Incrementable, Skills...>;
    static constexpr bool has_decrementable = detail::contains_v<Decrementable, Skills...>;
    static constexpr bool has_hashable = detail::contains_v<Hashable, Skills...>;
    static constexpr bool has_formattable = detail::contains_v<Formattable, Skills...>;
    static constexpr bool has_arithmetic = detail::contains_v<Arithmetic, Skills...>;
    static constexpr bool has_saturating_arithmetic = detail::contains_v<SaturatingArithmetic, Skills...>;

    constexpr strong_type()
        requires std::default_initializable<T>
    = default;
    constexpr strong_type(const strong_type&) = default;
    constexpr strong_type(strong_type&&) noexcept(std::is_nothrow_move_constructible_v<T>) = default;
    constexpr strong_type& operator=(const strong_type&) = default;
    constexpr strong_type& operator=(strong_type&&) noexcept(std::is_nothrow_move_assignable_v<T>) = default;
    ~strong_type() = default;

    template <class U>
        requires std::constructible_from<T, U&&>
    constexpr explicit strong_type(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>)
        : value_(std::forward<U>(value))
    {
    }

    [[nodiscard]] constexpr T& raw() & noexcept
    {
        return value_;
    }

    [[nodiscard]] constexpr const T& raw() const& noexcept
    {
        return value_;
    }

    [[nodiscard]] constexpr T&& raw() && noexcept
    {
        return std::move(value_);
    }

    [[nodiscard]] constexpr const T&& raw() const&& noexcept
    {
        return std::move(value_);
    }

    [[nodiscard]] constexpr explicit operator T&() & noexcept
    {
        return value_;
    }

    [[nodiscard]] constexpr explicit operator const T&() const& noexcept
    {
        return value_;
    }

    [[nodiscard]] constexpr explicit operator T&&() && noexcept
    {
        return std::move(value_);
    }

    [[nodiscard]] constexpr explicit operator const T&&() const&& noexcept
    {
        return std::move(value_);
    }

    // Comparison

    friend constexpr auto operator<=>(const strong_type& lhs, const strong_type& rhs)
        requires(has_comparable && std::three_way_comparable<T>)
    {
        return lhs.value_ <=> rhs.value_;
    }

    friend constexpr bool operator==(const strong_type& lhs, const strong_type& rhs)
        requires(has_comparable && std::equality_comparable<T>)
    {
        return lhs.value_ == rhs.value_;
    }

    // Additive / subtractive

    friend constexpr strong_type& operator+=(strong_type& lhs, const strong_type& rhs)
        requires(has_additive && requires(T& a, const T& b) { a += b; })
    {
        lhs.value_ += rhs.value_;
        return lhs;
    }

    friend constexpr strong_type operator+(strong_type lhs, const strong_type& rhs)
        requires(has_additive && requires(T& a, const T& b) { a += b; })
    {
        lhs += rhs;
        return lhs;
    }

    friend constexpr strong_type& operator-=(strong_type& lhs, const strong_type& rhs)
        requires(has_subtractive && requires(T& a, const T& b) { a -= b; })
    {
        lhs.value_ -= rhs.value_;
        return lhs;
    }

    friend constexpr strong_type operator-(strong_type lhs, const strong_type& rhs)
        requires(has_subtractive && requires(T& a, const T& b) { a -= b; })
    {
        lhs -= rhs;
        return lhs;
    }

    // Increment / decrement

    friend constexpr strong_type& operator++(strong_type& value)
        requires(has_incrementable && requires(T& v) { ++v; })
    {
        ++value.value_;
        return value;
    }

    friend constexpr strong_type operator++(strong_type& value, int)
        requires(has_incrementable && requires(T& v) { v++; })
    {
        auto copy = value;
        ++value;
        return copy;
    }

    friend constexpr strong_type& operator--(strong_type& value)
        requires(has_decrementable && requires(T& v) { --v; })
    {
        --value.value_;
        return value;
    }

    friend constexpr strong_type operator--(strong_type& value, int)
        requires(has_decrementable && requires(T& v) { v--; })
    {
        auto copy = value;
        --value;
        return copy;
    }

    // Multiplicative arithmetic

    friend constexpr strong_type& operator*=(strong_type& lhs, const T& rhs)
        requires(has_arithmetic && requires(T& a, const T& b) { a *= b; })
    {
        lhs.value_ *= rhs;
        return lhs;
    }

    friend constexpr strong_type operator*(strong_type lhs, const T& rhs)
        requires(has_arithmetic && requires(T& a, const T& b) { a *= b; })
    {
        lhs *= rhs;
        return lhs;
    }

    friend constexpr strong_type operator*(const T& lhs, strong_type rhs)
        requires(has_arithmetic && requires(T& a, const T& b) { a *= b; })
    {
        rhs *= lhs;
        return rhs;
    }

    friend constexpr strong_type& operator/=(strong_type& lhs, const T& rhs)
        requires(has_arithmetic && requires(T& a, const T& b) { a /= b; })
    {
        lhs.value_ /= rhs;
        return lhs;
    }

    friend constexpr strong_type operator/(strong_type lhs, const T& rhs)
        requires(has_arithmetic && requires(T& a, const T& b) { a /= b; })
    {
        lhs /= rhs;
        return lhs;
    }

    // -------------------------------------------------------------------------
    // Saturating arithmetic
    // -------------------------------------------------------------------------

    friend constexpr strong_type& saturating_add_assign(strong_type& lhs, const strong_type& rhs)
        requires(has_saturating_arithmetic)
    {
        lhs.value_ = detail::saturating_add(lhs.value_, rhs.value_);
        return lhs;
    }

    friend constexpr strong_type saturating_add(strong_type lhs, const strong_type& rhs)
        requires(has_saturating_arithmetic)
    {
        lhs.value_ = detail::saturating_add(lhs.value_, rhs.value_);
        return lhs;
    }

    friend constexpr strong_type& saturating_sub_assign(strong_type& lhs, const strong_type& rhs)
        requires(has_saturating_arithmetic)
    {
        lhs.value_ = detail::saturating_sub(lhs.value_, rhs.value_);
        return lhs;
    }

    friend constexpr strong_type saturating_sub(strong_type lhs, const strong_type& rhs)
        requires(has_saturating_arithmetic)
    {
        lhs.value_ = detail::saturating_sub(lhs.value_, rhs.value_);
        return lhs;
    }

  private:
    [[no_unique_address]] T value_{};
};

} // namespace cm

namespace std {

template <class T, class Tag, class... Skills, class CharT>
    requires cm::detail::contains_v<cm::Formattable, Skills...> && std::formattable<T, CharT>
struct formatter<cm::strong_type<T, Tag, Skills...>, CharT>
{
    formatter<T, CharT> base_{};

    constexpr auto parse(basic_format_parse_context<CharT>& ctx)
    {
        return base_.parse(ctx);
    }

    template <class FormatContext>
    auto format(const cm::strong_type<T, Tag, Skills...>& value, FormatContext& ctx) const
    {
        return base_.format(value.raw(), ctx);
    }
};

template <class T, class Tag, class... Skills>
    requires cm::detail::contains_v<cm::Hashable, Skills...> && requires(const T& value) {
        { std::hash<T>{}(value) } -> std::convertible_to<std::size_t>;
    }
struct hash<cm::strong_type<T, Tag, Skills...>>
{
    constexpr std::size_t operator()(const cm::strong_type<T, Tag, Skills...>& value) const
        noexcept(noexcept(std::hash<T>{}(value.raw())))
    {
        return std::hash<T>{}(value.raw());
    }
};

} // namespace std
