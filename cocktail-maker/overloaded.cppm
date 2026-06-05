module;

export module cm:overloaded;

namespace cm::detail {
export template <class... Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...;
};
} // namespace cm::detail