// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <utility>

namespace cm {
class ScopedAction
{
  public:
    ScopedAction() noexcept = default;
    ScopedAction(const ScopedAction&) = delete;
    ScopedAction& operator=(const ScopedAction&) = delete;
    ScopedAction& operator=(ScopedAction&& other) noexcept = delete;

    ScopedAction(ScopedAction&& other) noexcept
        : fn_(std::exchange(other.fn_, nullptr))
    {
    }

    template <class F>
    explicit ScopedAction(F&& f) // NOLINT(bugprone-forwarding-reference-overload)
        : fn_(std::forward<F>(f))
    {
    }

    ~ScopedAction() noexcept
    {
        run_and_clear();
    }

    // run immediately and prevent destructor from running again
    void run_now() noexcept
    {
        run_and_clear();
    }

  private:
    void run_and_clear() noexcept
    {
        if (fn_) {
            fn_();
            fn_ = nullptr;
        }
    }

    std::function<void()> fn_;
};

} // namespace cm
