#pragma once
namespace cm
{
template <class... Ts>
struct overloads : Ts... // NOLINT
{
    using Ts::operator()...;
};
} // namespace cm
