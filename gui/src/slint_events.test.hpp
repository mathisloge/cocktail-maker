#pragma once

// Helpers for tests that drive the Slint event loop. Include it after the imports, like fake_dispenser.test.hpp.

/// Runs everything that is queued on the Slint event loop so far.
inline void flush_slint_events()
{
    slint::invoke_from_event_loop([]() { slint::quit_event_loop(); });
    slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
}

/**
 * Flushes the Slint event loop when it goes away.
 *
 * Declared after the objects of a test, it runs last, so the cleanup they queue on the loop runs while what it
 * captures is still alive.
 */
struct SlintFlusher
{
    SlintFlusher() = default;
    SlintFlusher(const SlintFlusher&) = delete;
    SlintFlusher& operator=(const SlintFlusher&) = delete;

    ~SlintFlusher()
    {
        flush_slint_events();
    }
};
