module;
export module cm:pod_id;

import std;
import :strong_type;

namespace cm {
export using PodId = strong_type<std::string, struct PodIdTag, Comparable, Hashable, Formattable>;
}
