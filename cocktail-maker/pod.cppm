module;
#include <boost/cobalt.hpp>

export module cm:pod;

import std;
import :strong_type;

namespace cobalt = boost::cobalt;

namespace cm {

export using PodId = strong_type<int, struct PodIdTag, Comparable, Incrementable, Decrementable, Hashable, Formattable>;

export class Pod
{
  public:
};
} // namespace cm
