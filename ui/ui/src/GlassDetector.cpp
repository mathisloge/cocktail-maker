#include "cm/ui/GlassDetector.hpp"

namespace cm::ui {

cm::ui::GlassAdapter GlassDetector::detected_glass() const
{
    if (glass_store_ != nullptr and not glass_store_->glasses().empty()) {
        return glass_store_->glasses().begin()->second;
    }
    return {};
}

GlassStore* GlassDetector::glass_store()
{
    return glass_store_.get();
}

void GlassDetector::set_glass_store(GlassStore* glass_store)
{
    if (glass_store_.get() != glass_store) {
        glass_store_ = (glass_store != nullptr) ? glass_store->shared_from_this() : nullptr;
        Q_EMIT glass_store_changed();
        Q_EMIT detected_glass_changed();
    }
}
} // namespace cm::ui
