module;
#include <slint.h>
#include <spdlog/details/circular_q.h>
#include <spdlog/details/log_msg_buffer.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#include "app-window.h"

export module cm.gui:ui_log_sink;
import std;

namespace cm::gui {

export class UiLogModel : public slint::Model<LogEntry>
{
  public:
    UiLogModel()
        : q_{100}
    {
    }

    size_t row_count() const override
    {
        return q_.size();
    }

    std::optional<LogEntry> row_data(size_t i) const override
    {
        if (i < q_.size()) {
            return q_.at(i);
        }
        return std::nullopt;
    }

    void add_log_entry(LogEntry entry)
    {
        if (q_.full()) {
            q_.pop_front();
            notify_row_removed(0, 1);
        }
        q_.push_back(std::move(entry));
        notify_row_added(q_.size() - 1, 1);
    }

  private:
    spdlog::details::circular_q<LogEntry> q_;
};

export template <typename Mutex>
class UiLogSink : public spdlog::sinks::base_sink<Mutex>
{
  public:
    UiLogSink()
        : model_{std::make_shared<UiLogModel>()}
    {
    }

    std::shared_ptr<slint::Model<LogEntry>> model() const
    {
        return model_;
    }

  protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        slint::invoke_from_event_loop([model = model_, msg = transform(msg)]() mutable { model->add_log_entry(std::move(msg)); });
    }

    void flush_() override
    {
    }

  private:
    LogEntry transform(const spdlog::details::log_msg& msg) const
    {
        auto formatted_time = std::format("{}", msg.time);
        return LogEntry{
            .time_stamp = slint::SharedString{formatted_time.c_str()},
            .level = LogLevel::Ok,
            .message = slint::SharedString{msg.payload},
        };
    }

  private:
    std::shared_ptr<UiLogModel> model_;
};

export using ui_log_sink_mt = UiLogSink<std::mutex>;
export using ui_log_sink_st = UiLogSink<spdlog::details::null_mutex>;
} // namespace cm::gui
