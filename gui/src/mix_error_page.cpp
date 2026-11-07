module;
#include <proto/field/ErrorCodeCommon.h>

module cm.gui;

import std;
import cm;

namespace cm::gui {
namespace {
// codespell:ignore-begin
struct CustomerFacingText
{
    std::string title;
    std::string message;
};

CustomerFacingText customer_text_for(ErrorItemCategory category)
{
    switch (category) {
    case ErrorItemCategory::Ingredient:
        return {"Zutat nicht verfügbar", "Bitte wende dich kurz an das Personal."};
    case ErrorItemCategory::Abort:
        return {"Abgebrochen", "Die Zubereitung wurde abgebrochen."};
    case ErrorItemCategory::Unknown:
        return {"Kurze Unterbrechung", "Es gab ein technisches Problem. Bitte wende dich an das Personal."};
    }
    return {"Kurze Unterbrechung", "Es gab ein technisches Problem. Bitte wende dich an das Personal."};
}

namespace error_page {
MixErrorPage make(const DispenserEmptyError& ex, const cm::StationConfig& station_config, const IngredientStore& ingredient_store)
{
    const auto dispenser = PodDispenser{ex.pod_id(), ex.dispenser_id()};

    const std::expected<Ingredient, std::out_of_range> ingredient =
        station_config.find_ingredient_by_dispenser(dispenser).and_then(
            [&](IngredientId id) -> std::expected<Ingredient, std::out_of_range> {
                if (auto opt = ingredient_store.find_by_id(id); opt.has_value()) {
                    return std::move(opt).value();
                }
                return std::unexpected(std::out_of_range{"Konnte Ingredient für Behälter nicht finden"});
            });

    const auto item_details = std::format("Bitte Behälter '{}' von Pod '{}' befüllen", ex.dispenser_id(), ex.pod_id());

    if (!ingredient.has_value()) {
        // Dispenser->ingredient mapping missing or ingredient not in store -
        // still report the dispenser-empty error, just without a friendly name.
        return MixErrorPage{
            .technical_title = "Behälter leer!",
            .technical_description = "Bitte auffüllen.",
            .item_category = ErrorItemCategory::Ingredient,
            .item_name = "Unbekannte Zutat",
            .item_details = item_details,
        };
    }

    return MixErrorPage{
        .technical_title = "Behälter leer!",
        .technical_description = "Bitte auffüllen.",
        .item_category = ErrorItemCategory::Ingredient,
        .item_name = ingredient->display_name,
        .item_details = item_details,
    };
}

MixErrorPage make(const AbortError& ex, const cm::StationConfig&, const IngredientStore&)
{
    return MixErrorPage{
        .technical_title = "Prozess abgebrochen!",
        .technical_description = ex.what(),
        .item_category = ErrorItemCategory::Abort,
        .item_name = "Nutzer",
        .item_details = "Bitte neustarten.",
    };
}

MixErrorPage make(const RecipeNotFoundError&, const cm::StationConfig&, const IngredientStore&)
{
    return MixErrorPage{
        .technical_title = "Rezept nicht gefunden!",
        .technical_description = "Das ausgewählte Rezept konnte nicht geladen werden.",
        .item_category = ErrorItemCategory::Unknown,
        .item_name = "Rezept",
        .item_details = "Bitte ein anderes Rezept wählen oder die Rezeptdaten prüfen.",
    };
}

MixErrorPage make(const DispenserNotFoundError& ex, const cm::StationConfig&, const IngredientStore&)
{
    const auto item_details = std::format(
        "Pod '{}', Dispenser '{}' - Bitte Stationskonfiguration und Verkabelung prüfen.", ex.pod_id(), ex.dispenser_id());
    return MixErrorPage{
        .technical_title = "Dispenser nicht gefunden!",
        .technical_description = "Der für diese Zutat konfigurierte Dispenser konnte am Pod nicht gefunden werden.",
        .item_category = ErrorItemCategory::Unknown,
        .item_name = "Konfiguration",
        .item_details = item_details,
    };
}

MixErrorPage make(const IngredientNotAssignedError& ex, const cm::StationConfig&, const IngredientStore& ingredient_store)
{
    const auto ingredient = ingredient_store.find_by_id(ex.ingredient_id());
    const auto item_name = ingredient.has_value() ? ingredient->display_name : std::string{ex.ingredient_id().raw()};

    return MixErrorPage{
        .technical_title = "Zutat nicht zugeordnet!",
        .technical_description = "Für diese Zutat ist kein Dispenser konfiguriert.",
        .item_category = ErrorItemCategory::Ingredient,
        .item_name = item_name,
        .item_details = "Bitte in der Administration einen Dispenser zuweisen.",
    };
}

MixErrorPage make(const PodReceiveError& ex, const cm::StationConfig&, const IngredientStore&)
{
    using ErrorCode = proto::field::ErrorCodeCommon::ValueType;
    const auto pod_label = std::format("Pod '{}'", ex.pod_id());

    switch (ex.error_code()) {
    case ErrorCode::HardwareFault: {
        const auto item_details = std::format("{} - Bitte Pod prüfen und Support kontaktieren.", pod_label);
        return MixErrorPage{
            .technical_title = "Hardwarefehler!",
            .technical_description = "Der Pod meldet einen Hardwaredefekt.",
            .item_category = ErrorItemCategory::Unknown,
            .item_name = "Hardware",
            .item_details = item_details,
        };
    }
    case ErrorCode::Busy: {
        const auto item_details = std::format("{} - Bitte kurz warten und erneut versuchen.", pod_label);
        return MixErrorPage{
            .technical_title = "Pod beschäftigt!",
            .technical_description = "Der Pod verarbeitet noch einen anderen Befehl.",
            .item_category = ErrorItemCategory::Unknown,
            .item_name = "Pod",
            .item_details = item_details,
        };
    }
    case ErrorCode::NotCalibrated: {
        const auto item_details = std::format("{} - Bitte den Dispenser in den Einstellungen kalibrieren.", pod_label);
        return MixErrorPage{
            .technical_title = "Nicht kalibriert!",
            .technical_description = "Der angesprochene Dispenser wurde noch nicht kalibriert.",
            .item_category = ErrorItemCategory::Unknown,
            .item_name = "Kalibrierung",
            .item_details = item_details,
        };
    }
    case ErrorCode::UnsupportedInCurrentState: {
        const auto item_details = std::format("{} - Bitte Pod neustarten und erneut versuchen.", pod_label);
        return MixErrorPage{
            .technical_title = "Aktion nicht möglich!",
            .technical_description = "Der Pod befindet sich in einem Zustand, der diesen Befehl aktuell nicht zulässt.",
            .item_category = ErrorItemCategory::Unknown,
            .item_name = "Pod",
            .item_details = item_details,
        };
    }
    case ErrorCode::DispenserNotFound: {
        const auto item_details = std::format("{} - Bitte Stationskonfiguration prüfen.", pod_label);
        return MixErrorPage{
            .technical_title = "Dispenser unbekannt!",
            .technical_description = "Der Pod kennt den angesprochenen Dispenser nicht.",
            .item_category = ErrorItemCategory::Unknown,
            .item_name = "Dispenser",
            .item_details = item_details,
        };
    }
    case ErrorCode::NoLoadCell: {
        const auto item_details = std::format("{} - Bitte die Zutat einem Dispenser mit Wägezelle zuweisen.", pod_label);
        return MixErrorPage{
            .technical_title = "Keine Wägezelle!",
            .technical_description = "Der Dispenser hat keine Wägezelle und kann deshalb nicht dosieren.",
            .item_category = ErrorItemCategory::Unknown,
            .item_name = "Dispenser",
            .item_details = item_details,
        };
    }
    case ErrorCode::InvalidParameter:
    case ErrorCode::InternalError:
    case ErrorCode::DispenserEmpty:
    case ErrorCode::LoadNotSteady:
    case ErrorCode::TareTooOld:
    case ErrorCode::ImplausibleReference:
    case ErrorCode::ValuesLimit:
        break;
    }

    const auto item_details = std::format("{} - Bitte Support kontaktieren.", pod_label);
    return MixErrorPage{
        .technical_title = "Unbekannter Pod-Fehler!",
        .technical_description = "Der Pod hat einen unerwarteten Fehler gemeldet.",
        .item_category = ErrorItemCategory::Unknown,
        .item_name = "Pod",
        .item_details = item_details,
    };
}

MixErrorPage make(const PodTimeoutError& ex, const cm::StationConfig&, const IngredientStore&)
{
    const auto item_details = std::format("Pod '{}' - Bitte USB-Verbindung prüfen und den Pod neu starten.", ex.pod_id());
    return MixErrorPage{
        .technical_title = "Pod antwortet nicht!",
        .technical_description = "Der Pod hat innerhalb der erwarteten Zeit nicht reagiert.",
        .item_category = ErrorItemCategory::Unknown,
        .item_name = "Verbindung",
        .item_details = item_details,
    };
}

MixErrorPage make(const PodDisconnectedError& ex, const cm::StationConfig&, const IngredientStore&)
{
    const auto item_details = std::format("Pod '{}' - Bitte USB-Verbindung prüfen.", ex.pod_id());
    return MixErrorPage{
        .technical_title = "Pod nicht verbunden!",
        .technical_description = "Die Verbindung zum Pod ist abgebrochen.",
        .item_category = ErrorItemCategory::Unknown,
        .item_name = "Verbindung",
        .item_details = item_details,
    };
}

MixErrorPage make(const ProtocolError&, const cm::StationConfig&, const IngredientStore&)
{
    return MixErrorPage{
        .technical_title = "Kommunikationsfehler!",
        .technical_description = "Die Nachricht an den Pod konnte nicht verarbeitet werden.",
        .item_category = ErrorItemCategory::Unknown,
        .item_name = "Verbindung",
        .item_details = "Bitte USB-Verbindung prüfen. Besteht das Problem weiterhin, Support kontaktieren.",
    };
}

// Fallback for any exception without a dedicated overload above.
MixErrorPage make(const std::exception& ex, const cm::StationConfig&, const IngredientStore&)
{
    const auto item_details = std::format("Bitte Support kontaktieren. Details: {}", ex.what());
    return MixErrorPage{
        .technical_title = "Unbekannter Fehler!",
        .technical_description = "Es ist ein unerwarteter Fehler aufgetreten.",
        .item_category = ErrorItemCategory::Unknown,
        .item_name = "Fehlerdetails",
        .item_details = item_details,
    };
}

} // namespace error_page

// codespell:ignore-end
} // namespace

MixErrorPage mix_error_page(const std::exception_ptr& error,
                            const StationConfig& station_config,
                            const IngredientStore& ingredient_store)
{
    auto page = [&]() -> MixErrorPage {
        try {
            std::rethrow_exception(error);
        }
        catch (const DispenserEmptyError& ex) {
            return error_page::make(ex, station_config, ingredient_store);
        }
        catch (const DispenserNotFoundError& ex) {
            return error_page::make(ex, station_config, ingredient_store);
        }
        catch (const IngredientNotAssignedError& ex) {
            return error_page::make(ex, station_config, ingredient_store);
        }
        catch (const PodReceiveError& ex) {
            return error_page::make(ex, station_config, ingredient_store);
        }
        catch (const PodTimeoutError& ex) {
            return error_page::make(ex, station_config, ingredient_store);
        }
        catch (const PodDisconnectedError& ex) {
            return error_page::make(ex, station_config, ingredient_store);
        }
        catch (const ProtocolError& ex) {
            return error_page::make(ex, station_config, ingredient_store);
        }
        catch (const AbortError& ex) {
            return error_page::make(ex, station_config, ingredient_store);
        }
        catch (const RecipeNotFoundError& ex) {
            return error_page::make(ex, station_config, ingredient_store);
        }
        catch (const std::exception& ex) {
            return error_page::make(ex, station_config, ingredient_store);
        }
    }();

    auto customer_text = customer_text_for(page.item_category);
    page.customer_title = std::move(customer_text.title);
    page.customer_message = std::move(customer_text.message);
    return page;
}
} // namespace cm::gui
