#ifndef RE_NAME_RESOLVE_H
#define RE_NAME_RESOLVE_H

#include <UCD/ucd_compiler.hpp>

namespace re {

    class RE;
    class Name;

    RE * resolveUnicodeProperties(RE * re);
    RE * resolveNames(RE * re);

}
#endif
