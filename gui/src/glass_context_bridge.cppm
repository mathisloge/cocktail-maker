module;
#include <libassert/assert-macros.hpp>
#include <mp-units/systems/si/units.h>
#include <slint.h>
#include "app-window.h"

export module cm.gui:glass_context_bridge;

import std;
import libassert;
import cm.core;
import cm;

namespace cm::gui {

auto to_litre(int volume_ml) -> units::Litre
{
    return units::Litre{volume_ml * units::milli_litre};
}

auto format_ml(const units::Litre volume) -> slint::SharedString
{
    const auto ml = volume.numerical_value_in(units::milli_litre);
    return slint::SharedString(std::format("{}", std::llround(ml)));
}

auto to_string_model(const std::vector<std::string>& values) -> std::shared_ptr<slint::Model<slint::SharedString>>
{
    std::vector<slint::SharedString> shared_values(values.begin(), values.end());
    return std::make_shared<slint::VectorModel<slint::SharedString>>(std::move(shared_values));
}

auto to_ml_model(const std::vector<units::Litre>& volumes) -> std::shared_ptr<slint::Model<slint::SharedString>>
{
    std::vector<slint::SharedString> shared_values;
    shared_values.reserve(volumes.size());
    for (auto const& v : volumes) {
        shared_values.emplace_back(format_ml(v));
    }
    return std::make_shared<slint::VectorModel<slint::SharedString>>(std::move(shared_values));
}

auto transform(const cm::GlassIconData& icon) -> gui::GlassIconData
{
    return gui::GlassIconData{
        .viewbox_width = icon.viewbox_width,
        .viewbox_height = icon.viewbox_height,
        .outline = slint::SharedString{icon.outline.c_str()},
        .rim = to_string_model(icon.rim),
    };
}

export class VolumeModel : public slint::Model<slint::SharedString>
{
  public:
    explicit VolumeModel(std::vector<units::Litre> volumes)
        : volumes_{std::move(volumes)}
    {
        std::ranges::sort(volumes_);
    }

    size_t row_count() const override
    {
        return volumes_.size();
    }

    std::optional<slint::SharedString> row_data(size_t i) const override
    {
        if (i >= volumes_.size()) {
            return std::nullopt;
        }
        return format_ml(volumes_[i]);
    }

    void add(units::Litre volume)
    {
        auto it = std::ranges::lower_bound(volumes_, volume);
        if (it != volumes_.end() && *it == volume) {
            return;
        }
        const auto index = static_cast<size_t>(std::distance(volumes_.begin(), it));
        volumes_.insert(it, volume);
        notify_row_added(index, 1);
    }

    void remove(units::Litre volume)
    {
        auto it = std::ranges::find(volumes_, volume);
        if (it == volumes_.end()) {
            return;
        }
        const auto index = static_cast<size_t>(std::distance(volumes_.begin(), it));
        volumes_.erase(it);
        notify_row_removed(index, 1);
    }

  private:
    std::vector<units::Litre> volumes_;
};

export class GlassListModel : public slint::Model<GlassDescriptor>
{
  public:
    size_t row_count() const override
    {
        return entries_.size();
    }

    std::optional<GlassDescriptor> row_data(size_t i) const override
    {
        if (i >= entries_.size()) {
            return std::nullopt;
        }
        return to_descriptor(entries_[i]);
    }

    // Fügt das Glas hinzu, falls es noch nicht angezeigt wird.
    void add(const GlassId& glass_id, const Glass& glass)
    {
        if (find_index(glass_id)) {
            return;
        }
        entries_.emplace_back(Entry{
            .id = glass_id,
            .display_name = slint::SharedString{glass.display_name},
            .common_volumes = to_ml_model(glass.common_volumes),
            .active_volumes = std::make_shared<VolumeModel>(glass.active_volumes),
            .icon = transform(glass.icon),
        });
        notify_row_added(entries_.size() - 1, 1);
    }

    void remove(const GlassId& glass_id)
    {
        if (auto index = find_index(glass_id)) {
            entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(*index));
            notify_row_removed(*index, 1);
        }
    }

    // Gibt das mutierbare VolumeModel für ein angezeigtes Glas zurück, oder nullptr.
    [[nodiscard]] std::shared_ptr<VolumeModel> volume_model(const GlassId& glass_id) const
    {
        if (auto index = find_index(glass_id)) {
            return entries_[*index].active_volumes;
        }
        return nullptr;
    }

  private:
    struct Entry
    {
        GlassId id;
        slint::SharedString display_name;
        std::shared_ptr<slint::Model<slint::SharedString>> common_volumes;
        std::shared_ptr<VolumeModel> active_volumes;
        gui::GlassIconData icon;
    };

    [[nodiscard]] std::optional<size_t> find_index(const GlassId& glass_id) const
    {
        auto it = std::ranges::find(entries_, glass_id, &Entry::id);
        if (it == entries_.end()) {
            return std::nullopt;
        }
        return static_cast<size_t>(std::distance(entries_.begin(), it));
    }

    static GlassDescriptor to_descriptor(const Entry& e)
    {
        return GlassDescriptor{
            .id = slint::SharedString{e.id.raw().c_str()},
            .display_name = e.display_name,
            .common_volumes_ml = e.common_volumes,
            .active_volumes_ml = e.active_volumes,
            .icon = e.icon,
        };
    }

  private:
    std::vector<Entry> entries_;
};

export class GlassContextBridge
{
  public:
    GlassContextBridge(slint::ComponentHandle<AppWindow> ui)
        : ui_{std::move(ui)}
    {
    }

    void init()
    {
        store_->init_from_dir("");
        register_ui_callbacks();
    }

  private:
    void register_ui_callbacks();
    void on_ui_add_glass(slint::SharedString glass_id);
    void on_ui_remove_glass(slint::SharedString glass_id);
    void on_ui_add_glass_volume(slint::SharedString glass_id, int volume_ml);
    void on_ui_remove_glass_volume(slint::SharedString glass_id, int volume_ml);

  private:
    slint::ComponentHandle<AppWindow> ui_;
    std::shared_ptr<GlassStore> store_ = std::make_shared<GlassStore>(); // TODO: move store into application
    std::shared_ptr<GlassListModel> glasses_ = std::make_shared<GlassListModel>();
};

void GlassContextBridge::register_ui_callbacks()
{
    auto&& ctx = ui_->global<GlassContext>();
    ctx.set_glasses(glasses_);
    ctx.on_add_glass(std::bind(&GlassContextBridge::on_ui_add_glass, this, std::placeholders::_1));
    ctx.on_remove_glass(std::bind(&GlassContextBridge::on_ui_remove_glass, this, std::placeholders::_1));
    ctx.on_add_volume_from_glass(
        std::bind(&GlassContextBridge::on_ui_add_glass_volume, this, std::placeholders::_1, std::placeholders::_2));
    ctx.on_remove_volume_from_glass(
        std::bind(&GlassContextBridge::on_ui_remove_glass_volume, this, std::placeholders::_1, std::placeholders::_2));
}

void GlassContextBridge::on_ui_add_glass(slint::SharedString glass_id)
{
    auto it = store_->glasses().find(GlassId{glass_id});
    if (it == store_->glasses().end()) {
        return;
    }
    glasses_->add(it->first, it->second);
}

void GlassContextBridge::on_ui_remove_glass(slint::SharedString glass_id)
{
    glasses_->remove(GlassId{glass_id});
}

void GlassContextBridge::on_ui_add_glass_volume(slint::SharedString glass_id, int volume_ml)
{
    if (auto volumes = glasses_->volume_model(GlassId{glass_id})) {
        volumes->add(to_litre(volume_ml));
    }
}

void GlassContextBridge::on_ui_remove_glass_volume(slint::SharedString glass_id, int volume_ml)
{
    if (auto volumes = glasses_->volume_model(GlassId{glass_id})) {
        volumes->remove(to_litre(volume_ml));
    }
}

} // namespace cm::gui
