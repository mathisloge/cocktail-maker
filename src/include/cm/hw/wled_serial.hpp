#pragma once
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/asio/strand.hpp>
#include <nlohmann/json_fwd.hpp>
#include <spdlog/fwd.h>
#include "cm/async.hpp"
#include "cm/units.hpp"

namespace cm {

class WledSerial : public std::enable_shared_from_this<WledSerial>
{
  public:
    enum class State
    {
        none,
        inactive,        //! solid + blue
        active,          //! meteor effect + rainbow
        empty_ingredient //! heartbeat + orangery
    };

    struct SegmentRange
    {
        std::uint16_t start;
        std::uint16_t end;
    };

    struct LedSegment
    {
        std::uint8_t id;
        std::uint16_t start;
        std::uint16_t end;
        State state;
    };

    static std::shared_ptr<WledSerial> create(boost::asio::any_io_executor executor,
                                              const std::string& device,
                                              std::initializer_list<SegmentRange> segments);
    ~WledSerial();

    void shutdown();

    async<void> turn_on();
    async<void> turn_off();
    async<void> request_state();
    void set_state(std::uint8_t segment, State state);

  private:
    async<void> show_active();
    async<void> show_empty();
    async<void> show_progress(units::quantity<units::percent> progress);

  private:
    explicit WledSerial(boost::asio::any_io_executor executor,
                        const std::string& device,
                        std::initializer_list<SegmentRange> segments);
    void start_write_loop();
    async<void> write_loop();
    async<void> write_state();
    async<void> write(nlohmann::json json);

  private:
    std::unordered_map<std::uint8_t, LedSegment> segments_;
    std::shared_ptr<spdlog::logger> logger_;
    boost::asio::strand<boost::asio::any_io_executor> strand_;
    boost::asio::experimental::channel<void(boost::system::error_code)> channel_;
    boost::asio::serial_port port_;
    boost::asio::cancellation_signal cancel_signal_;
};
} // namespace cm
