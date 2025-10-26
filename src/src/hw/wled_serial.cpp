#include "cm/hw/wled_serial.hpp"
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <libassert/assert.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "cm/logging.hpp"

using json = nlohmann::json;

namespace cm {
namespace {

struct PowerCommand
{
    bool on;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PowerCommand, on)
};

struct WLEDColor
{
    uint8_t r;
    uint8_t g;
    uint8_t b;

    // Custom from_json for array format [r, g, b]
    friend void from_json(const json& j, WLEDColor& c)
    {
        if (j.size() >= 3) {
            c.r = j[0].get<uint8_t>();
            c.g = j[1].get<uint8_t>();
            c.b = j[2].get<uint8_t>();
        }
    }

    friend void to_json(json& j, const WLEDColor& c)
    {
        j = json::array({c.r, c.g, c.b});
    }
};

struct WLEDSegment
{
    int id;
    int start;
    int stop;
    int len;
    int grp;
    int spc;
    int of;
    bool on;
    bool frz;
    uint8_t bri;
    uint8_t cct;
    int set;
    int lc;
    std::vector<WLEDColor> col;
    int fx;
    int sx;
    int ix;
    int pal;
    int c1;
    int c2;
    int c3;
    bool sel;
    bool rev;
    bool mi;
    bool o1;
    bool o2;
    bool o3;
    int si;
    int m12;
    int bm;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(WLEDSegment,
                                   id,
                                   start,
                                   stop,
                                   len,
                                   grp,
                                   spc,
                                   of,
                                   on,
                                   frz,
                                   bri,
                                   cct,
                                   set,
                                   lc,
                                   col,
                                   fx,
                                   sx,
                                   ix,
                                   pal,
                                   c1,
                                   c2,
                                   c3,
                                   sel,
                                   rev,
                                   mi,
                                   o1,
                                   o2,
                                   o3,
                                   si,
                                   m12,
                                   bm)
};

struct WLEDNightlight
{
    bool on;
    int dur;
    int mode;
    int tbri;
    int rem;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(WLEDNightlight, on, dur, mode, tbri, rem)
};

struct WLEDUdpn
{
    bool send;
    bool recv;
    int sgrp;
    int rgrp;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(WLEDUdpn, send, recv, sgrp, rgrp)
};

struct WLEDState
{
    bool on;
    uint8_t bri;
    int transition;
    int bs;
    int ps;
    int pl;
    int ledmap;
    WLEDNightlight nl;
    WLEDUdpn udpn;
    int lor;
    int mainseg;
    std::vector<WLEDSegment> seg;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(WLEDState, on, bri, transition, bs, ps, pl, ledmap, nl, udpn, lor, mainseg, seg)
};

struct WLEDLeds
{
    int count;
    int pwr;
    int fps;
    int maxpwr;
    int maxseg;
    int bootps;
    std::vector<int> seglc;
    int lc;
    bool rgbw;
    int wv;
    int cct;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(WLEDLeds, count, pwr, fps, maxpwr, maxseg, bootps, seglc, lc, rgbw, wv, cct)
};

struct WLEDWifi
{
    std::string bssid;
    int rssi;
    int signal;
    int channel;
    bool ap;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(WLEDWifi, bssid, rssi, signal, channel, ap)
};

struct WLEDFs
{
    int u;
    int t;
    long long pmt;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(WLEDFs, u, t, pmt)
};

struct WLEDMap
{
    int id;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(WLEDMap, id)
};

struct WLEDInfo
{
    std::string ver;
    int vid;
    std::string cn;
    std::string release;
    std::string repo;
    WLEDLeds leds;
    bool str;
    std::string name;
    int udpport;
    bool simplifiedui;
    bool live;
    int liveseg;
    std::string lm;
    std::string lip;
    int ws;
    int fxcount;
    int palcount;
    int cpalcount;
    int cpalmax;
    std::vector<WLEDMap> maps;
    WLEDWifi wifi;
    WLEDFs fs;
    int ndc;
    std::string arch;
    std::string core;
    int clock;
    int flash;
    int lwip;
    int freeheap;
    int uptime;
    std::string time;
    std::vector<std::string> um;
    int opt;
    std::string brand;
    std::string product;
    std::string mac;
    std::string ip;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(WLEDInfo,
                                   ver,
                                   vid,
                                   cn,
                                   release,
                                   repo,
                                   leds,
                                   str,
                                   name,
                                   udpport,
                                   simplifiedui,
                                   live,
                                   liveseg,
                                   lm,
                                   lip,
                                   ws,
                                   fxcount,
                                   palcount,
                                   cpalcount,
                                   cpalmax,
                                   maps,
                                   wifi,
                                   fs,
                                   ndc,
                                   arch,
                                   core,
                                   clock,
                                   flash,
                                   lwip,
                                   freeheap,
                                   uptime,
                                   time,
                                   um,
                                   opt,
                                   brand,
                                   product,
                                   mac,
                                   ip)
};

struct WLEDResponse
{
    WLEDState state;
    WLEDInfo info;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(WLEDResponse, state, info)
};

constexpr std::uint8_t pallette_id(WledSerial::State state)
{
    switch (state) {
    case WledSerial::State::none:
        [[fallthrough]];
    case WledSerial::State::inactive:
        return 0;
    case WledSerial::State::active:
        return 11;
    case WledSerial::State::empty_ingredient:
        return 47;
    }
    return 0;
}

constexpr std::uint8_t effect_id(WledSerial::State state)
{
    switch (state) {
    case WledSerial::State::none:
        [[fallthrough]];
    case WledSerial::State::inactive:
        return 0;
    case WledSerial::State::active:
        return 76;
    case WledSerial::State::empty_ingredient:
        return 100;
    }
    return 0;
}

} // namespace

WledSerial::WledSerial(boost::asio::any_io_executor executor,
                       const std::string& device,
                       std::initializer_list<SegmentRange> segments)
    : logger_{LoggingContext::instance().create_logger(fmt::format("WledSerial@{}", device))}
    , strand_(boost::asio::make_strand(executor))
    , channel_{strand_, 1}
    , port_{strand_, device}

{
    std::uint8_t id{0};
    for (auto&& s : segments) {
        segments_.emplace(id, LedSegment{.id = id, .start = s.start, .end = s.end, .state = State{}});
        ++id;
    }
    port_.set_option(boost::asio::serial_port_base::baud_rate{115200});
    port_.set_option(boost::asio::serial_port_base::character_size{8});
    port_.set_option(boost::asio::serial_port_base::parity{boost::asio::serial_port_base::parity::none});
    port_.set_option(boost::asio::serial_port_base::stop_bits{boost::asio::serial_port_base::stop_bits::one});
    port_.set_option(boost::asio::serial_port_base::flow_control{boost::asio::serial_port_base::flow_control::none});
}

WledSerial::~WledSerial() = default;

std::shared_ptr<WledSerial> WledSerial::create(boost::asio::any_io_executor executor,
                                               const std::string& device,
                                               std::initializer_list<SegmentRange> segments)
{
    struct Enabler : public WledSerial
    {
      public:
        Enabler(boost::asio::any_io_executor executor, const std::string& device, std::initializer_list<SegmentRange> segments)
            : WledSerial{std::move(executor), device, segments}
        {
        }
    };

    auto instance = std::make_shared<Enabler>(std::move(executor), device, segments);
    instance->start_write_loop();
    return instance;
}

void WledSerial::shutdown()
{
    SPDLOG_LOGGER_INFO(logger_, "Requesting shutdown...");
    cancel_signal_.emit(boost::asio::cancellation_type::all);
}

void WledSerial::start_write_loop()
{
    boost::asio::co_spawn(
        strand_,
        [self = shared_from_this()] -> async<void> { co_await self->write_loop(); },
        boost::asio::bind_cancellation_slot(cancel_signal_.slot(), boost::asio::detached));

    reset();
}

async<void> WledSerial::write_loop()
{
    SPDLOG_LOGGER_INFO(logger_, "Starting...");
    try {
        while (true) {
            // Wait for write request
            co_await channel_.async_receive(boost::asio::use_awaitable);

            SPDLOG_LOGGER_DEBUG(logger_, "TEST");

            // Debounce
            co_await boost::asio::steady_timer{co_await boost::asio::this_coro::executor, std::chrono::milliseconds{100}}
                .async_wait(boost::asio::use_awaitable);

            // Drain channel
            while (channel_.try_receive([](...) {})) {
            }
            co_await write_state();
        }
    }
    catch (const boost::system::system_error& e) {
        if (e.code() == boost::asio::error::operation_aborted) {
            SPDLOG_LOGGER_INFO(logger_, "Write loop cancelled gracefully");
            co_return; // Clean cancellation
        }

        SPDLOG_LOGGER_CRITICAL(logger_,
                               "Write loop terminated with system error: {} (code: {}, category: {})",
                               e.what(),
                               e.code().value(),
                               e.code().category().name());
    }
    catch (const std::exception& ex) {
        SPDLOG_LOGGER_CRITICAL(logger_, "Write loop terminated with unexpected exception: {}", ex.what());
    }
    catch (...) {
        SPDLOG_LOGGER_CRITICAL(logger_, "Write loop terminated with unknown exception");
    }
}

void WledSerial::set_state(std::uint8_t segment, State state)
{
    boost::asio::post(strand_, [this, segment, state]() { set_state_impl(segment, state); });
}

// Should only be executed in strand.
void WledSerial::set_state_impl(std::uint8_t segment, State state)
{
    auto it = segments_.find(segment);
    ASSERT(it != segments_.end());
    const bool changed = it->second.state != state;
    it->second.state = state;
    if (changed) {
        channel_.try_send(boost::system::error_code{});
    }
}

void WledSerial::reset()
{
    boost::asio::post(strand_, [this]() {
        for (auto&& s : segments_) {
            set_state_impl(s.first, State::inactive);
        }
    });
}

async<void> WledSerial::write_state()
{
    json segments;
    for (auto&& [id, segment] : std::as_const(segments_)) {
        segments.emplace_back(json{{"id", id},
                                   {"start", segment.start},
                                   {"stop", segment.end},
                                   {"pal", pallette_id(segment.state)},
                                   {"fx", effect_id(segment.state)}});
    }
    co_await this->write(json{
        {"on", true},
        {"bri", 255},
        {"seg", std::move(segments)},
    });
}

async<void> WledSerial::turn_on()
{
    co_await this->write(json(PowerCommand{.on = true}));
}

async<void> WledSerial::turn_off()
{
    co_await this->write(json(PowerCommand{.on = false}));
}

async<void> WledSerial::show_progress(units::quantity<units::percent> progress)
{
    // json{{"on", true}, {"bri", 255}, {"seg", {{{"id", 0}, {"start", 0}, {"stop", 24}, {"fxdef", true}, {"pal", 0}, {"col", {{0,
    // 0, 255}}}, {"fx", 0}}}}});
    co_await this->write(json{
        {"on", true},
        {"bri", 255},
        {"seg",
         {
             {{"id", 0},
              {"start", 0},
              {"stop", 24},
              {"pal", 6},
              {"fx", 98},
              {"ix", units::value_cast<std::uint8_t>(progress).numerical_value_in(units::percent)}},
         }},
    });
}

async<void> WledSerial::show_active()
{
    co_await this->write(json{
        {"on", true},
        {"bri", 255},
        {"seg", {{{"id", 0}, {"start", 0}, {"stop", 24}, {"pal", 11}, {"fx", 76}}}},
    });
}

async<void> WledSerial::show_empty()
{
    co_await this->write(json{
        {"on", true},
        {"bri", 255},
        {"seg", {{{"id", 0}, {"start", 0}, {"stop", 24}, {"pal", 47}, {"fx", 100}}}},
    });
}

async<void> WledSerial::request_state()
{
    co_await this->write(json{{"v", true}});

    boost::asio::streambuf buffer;

    const size_t n = co_await boost::asio::async_read_until(
        port_, buffer, '\n', boost::asio::bind_cancellation_slot(cancel_signal_.slot(), boost::asio::use_awaitable));
    SPDLOG_LOGGER_DEBUG(logger_, "Received: {}", n);
    const std::string json_str(boost::asio::buffers_begin(buffer.data()), boost::asio::buffers_end(buffer.data()));
    try {
        const json j = json::parse(json_str);
        const auto state = j.get<WLEDResponse>();

        SPDLOG_LOGGER_DEBUG(logger_, "wled state: Version:{}", state.info.ver);
    }
    catch (const std::exception& ex) {
        SPDLOG_LOGGER_CRITICAL(logger_, "Could not parse received response: {}, {}", ex.what(), json_str);
    }
}

async<void> WledSerial::write(json json)
{
    const std::string msg = json.dump();
    SPDLOG_LOGGER_DEBUG(logger_, "Write: {}", msg);
    const auto written = co_await boost::asio::async_write(port_, boost::asio::buffer(msg), boost::asio::use_awaitable);
    ASSUME(msg.size() == written);
    co_return;
}
} // namespace cm
