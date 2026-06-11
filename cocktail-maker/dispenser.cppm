module;

export module cm:dispenser;
import :strong_type;

namespace cm {

export using DispenserId = strong_type<int, struct DispenserIdTag, Comparable, Hashable, Formattable>;

}
