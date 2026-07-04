module;

export module cm.core:overloaded;

namespace cm::detail {
export template <class... Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...;
};
} // namespace cm::detail
