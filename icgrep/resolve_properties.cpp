/*
 *  Copyright (c) 2015 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include <string>
#include <re/re_re.h>
#include <re/re_alt.h>
#include <re/re_any.h>
#include <re/re_cc.h>
#include <re/re_seq.h>
#include <re/re_rep.h>
#include <re/re_name.h>
#include <re/re_diff.h>
#include <re/re_intersect.h>
#include <re/re_assertion.h>
#include <re/re_start.h>
#include <re/re_end.h>
#include <cc/cc_namemap.hpp>
#include "UCD/PropertyAliases.h"
#include "UCD/PropertyObjects.h"
#include "UCD/PropertyObjectTable.h"
#include "UCD/PropertyValueAliases.h"
#include <boost/algorithm/string/case_conv.hpp>
#include "resolve_properties.h"

using namespace UCD;
using namespace re;

class UnicodePropertyExpressionError : public std::exception {
public:
    UnicodePropertyExpressionError(const std::string && msg) noexcept : _msg(msg) {}
    const char* what() const noexcept { return _msg.c_str();}
private:
    inline UnicodePropertyExpressionError() noexcept {}
    const std::string _msg;
};

inline std::string lowercase(const std::string & name) {
    std::locale loc;
    return boost::algorithm::to_lower_copy(name, loc);
}

inline int GetPropertyValueEnumCode(const UCD::property_t type, const std::string & value) {
    return property_object_table[type]->GetPropertyValueEnumCode(value);
}

void resolveProperties(RE * re) {
    if (Alt * alt = dyn_cast<Alt>(re)) {
        for (auto item : *alt) {
            resolveProperties(item);
        }
    }
    else if (Seq * seq = dyn_cast<Seq>(re)) {
        for (auto item : *seq) {
            resolveProperties(item);
        }
    }
    else if (Rep * rep = dyn_cast<Rep>(re)) {
        resolveProperties(rep->getRE());
    }
    else if (Assertion * a = dyn_cast<Assertion>(re)) {
        resolveProperties(a->getAsserted());
    }
    else if (Diff * diff = dyn_cast<Diff>(re)) {
        resolveProperties(diff->getRH());
        resolveProperties(diff->getLH());
    }
    else if (Intersect * e = dyn_cast<Intersect>(re)) {
        resolveProperties(e->getRH());
        resolveProperties(e->getLH());
    }
    else if (Name * name = dyn_cast<Name>(re)) {
        if (name->getType() == Name::Type::UnicodeProperty) {
            const std::string prop = canonicalize_value_name(name->getNamespace());
            const std::string value = canonicalize_value_name(name->getName());
            property_t theprop;
            if (prop.length() != 0) {
                auto propit = alias_map.find(prop);
                if (propit == alias_map.end()) {
                    throw UnicodePropertyExpressionError("Expected a property name, but '" + name->getNamespace() + "' found instead");
                }
                theprop = propit->second;
                if (theprop == gc) {
                    // General Category
                    int valcode = GetPropertyValueEnumCode(gc, value);
                    if (valcode < 0) {
                        throw UnicodePropertyExpressionError("Erroneous property value for general_category property");
                    }                    
                    name->setName("__get_gc_" + GC_ns::enum_names[valcode]);
                }
                else if (theprop == sc) {
                    // Script property identified
                    int valcode = GetPropertyValueEnumCode(sc, value);
                    if (valcode < 0) {
                        throw UnicodePropertyExpressionError("Erroneous property value for script property");
                    }
                    name->setName("__get_sc_" + SC_ns::enum_names[valcode]);
                }
                else if (theprop == scx) {
                    // Script extension property identified
                    int valcode = GetPropertyValueEnumCode(sc, value);
                    if (valcode >= 0) {
                        throw UnicodePropertyExpressionError("Erroneous property value for script_extension property");
                    }
                    name->setName("__get_scx_" + SC_ns::enum_names[valcode]);
                }
                else if (theprop == blk) {
                    // Block property identified
                    int valcode = GetPropertyValueEnumCode(blk, value);
                    if (valcode >= 0) {
                         throw UnicodePropertyExpressionError("Erroneous property value for block property");
                    }
                    name->setName("__get_blk_" + BLK_ns::enum_names[valcode]);
                }
                else if (property_object_table[theprop]->the_kind == PropertyObject::ClassTypeId::BinaryProperty){
                    auto valit = Binary_ns::aliases_only_map.find(value);
                    if (valit == Binary_ns::aliases_only_map.end()) {
                        throw UnicodePropertyExpressionError("Erroneous property value for binary property " + property_full_name[theprop]);
                    }
                    if (valit->second == Binary_ns::Y) {
                        name->setName("__get_" + lowercase(property_enum_name[theprop]) + "_Y");
                        return;
                    }
                    else {
                        Name * binprop = makeName("__get_" + lowercase(property_enum_name[theprop]) + "_Y", Name::Type::UnicodeProperty);
                        name->setDefinition(makeDiff(makeAny(), binprop));
                        return;
                    }
                }
                else {
                    throw UnicodePropertyExpressionError("Property " + property_full_name[theprop] + " recognized, but not supported in icgrep 1.0");
                }
            }
            else {
                // No namespace (property) name.   Try as a general category.
                int valcode = GetPropertyValueEnumCode(gc, value);
                if (valcode >= 0) {
                    theprop = gc;
                    name->setName("__get_gc_" + GC_ns::enum_names[valcode]);
                    return;
                }
                valcode = GetPropertyValueEnumCode(sc, value);
                if (valcode >= 0) {
                    theprop = sc;
                    name->setName("__get_sc_" + SC_ns::enum_names[valcode]);
                    return;
                }
                // Try as a binary property.
                auto propit = alias_map.find(value);
                if (propit != alias_map.end()) {
                    theprop = propit->second;
                    if (property_object_table[theprop]->the_kind == PropertyObject::ClassTypeId::BinaryProperty) {
                        name->setName("__get_" + lowercase(property_enum_name[theprop]) + "_Y");
                        return;
                    }
                    else {
                        throw UnicodePropertyExpressionError("Error: property " + property_full_name[theprop] + " specified without a value");
                    }
                }
                // Now try special cases of Unicode TR #18
                else if (value == "any") {
                    name->setDefinition(makeAny());
                    return;
                }
                else if (value == "assigned") {
                    Name * Cn = makeName("Cn", Name::Type::UnicodeProperty);
                    resolveProperties(Cn);
                    name->setDefinition(makeDiff(makeAny(), Cn));
                    return;
                }
                else if (value == "ascii") {
                    name->setName("__get_blk_ASCII");
                    return;
                }
                // Now compatibility properties of UTR #18 Annex C
                else if (value == "xdigit") {
                    Name * Nd = makeName("Nd", Name::Type::UnicodeProperty);
                    resolveProperties(Nd);
                    Name * hexdigit = makeName("Hex_digit", Name::Type::UnicodeProperty);
                    resolveProperties(hexdigit);
                    name->setDefinition(makeAlt({Nd, hexdigit}));
                    return;
                }
                else if (value == "alnum") {
                    Name * digit = makeName("Nd", Name::Type::UnicodeProperty);
                    resolveProperties(digit);
                    Name * alpha = makeName("alphabetic", Name::Type::UnicodeProperty);
                    resolveProperties(alpha);
                    name->setDefinition(makeAlt({digit, alpha}));
                    return;
                }
                else if (value == "blank") {
                    Name * space_sep = makeName("space_separator", Name::Type::UnicodeProperty);
                    resolveProperties(space_sep);
                    CC * tab = makeCC(0x09);
                    name->setDefinition(makeAlt({space_sep, tab}));
                    return;
                }
                else if (value == "graph") {
                    Name * space = makeName("space", Name::Type::UnicodeProperty);
                    resolveProperties(space);
                    Name * ctrl = makeName("control", Name::Type::UnicodeProperty);
                    resolveProperties(ctrl);
                    Name * surr = makeName("surrogate", Name::Type::UnicodeProperty);
                    resolveProperties(surr);
                    Name * unassigned = makeName("Cn", Name::Type::UnicodeProperty);
                    resolveProperties(unassigned);
                    Name * nongraph = makeName("[^graph]", Name::Type::UnicodeProperty);
                    nongraph->setDefinition(makeAlt({space, ctrl, surr, unassigned}));
                    name->setDefinition(makeDiff(makeAny(), nongraph));
                    return;
                }
                else if (value == "print") {
                    Name * graph = makeName("graph", Name::Type::UnicodeProperty);
                    resolveProperties(graph);
                    Name * space_sep = makeName("space_separator", Name::Type::UnicodeProperty);
                    resolveProperties(space_sep);
                    std::vector<RE *> alts = {graph, space_sep};
                    name->setDefinition(makeAlt(alts.begin(), alts.end()));
                    return;
                }
                else if (value == "word") {
                    Name * alnum = makeName("alnum", Name::Type::UnicodeProperty);
                    resolveProperties(alnum);
                    Name * mark = makeName("mark", Name::Type::UnicodeProperty);
                    resolveProperties(mark);
                    Name * conn = makeName("Connector_Punctuation", Name::Type::UnicodeProperty);
                    resolveProperties(conn);
                    Name * join = makeName("Join_Control", Name::Type::UnicodeProperty);
                    resolveProperties(join);
                    name->setDefinition(makeAlt({alnum, mark, conn, join}));
                    return;
                }
                else {
                    throw UnicodePropertyExpressionError("Expected a general category, script or binary property name, but '" + name->getName() + "' found instead");
                }
            }

            //name->setCompiled(compileCC(cast<CC>(d), mCG));
        }
    }
    else if (!isa<CC>(re) && !isa<Start>(re) && !isa<End>(re) && !isa<Any>(re)) {
        throw UnicodePropertyExpressionError("Unknown RE type in resolveProperties.");
    }
}

UnicodeSet resolveUnicodeSet(Name * const name) {

    if (name->getType() == Name::Type::UnicodeProperty) {
        std::string prop = name->getNamespace();
        std::string value = canonicalize_value_name(name->getName());
        property_t theprop;
        if (prop.length() > 0) {
            prop = canonicalize_value_name(prop);
            auto propit = alias_map.find(prop);
            if (propit == alias_map.end()) {
                throw UnicodePropertyExpressionError("Expected a property name, but '" + name->getNamespace() + "' found instead");
            }
            theprop = propit->second;
            if (theprop == gc) {
                // General Category
                return cast<EnumeratedPropertyObject>(property_object_table[gc])->GetCodepointSet(value);
            }
            else if (theprop == sc) {
                // Script property identified
                return cast<EnumeratedPropertyObject>(property_object_table[sc])->GetCodepointSet(value);
            }
            else if (theprop == scx) {
                // Script extension property identified
                return cast<EnumeratedPropertyObject>(property_object_table[sc])->GetCodepointSet(value);
            }
            else if (theprop == blk) {
                // Block property identified
                return cast<EnumeratedPropertyObject>(property_object_table[blk])->GetCodepointSet(value);
            }
            else if (property_object_table[theprop]->the_kind == PropertyObject::ClassTypeId::BinaryProperty){
                auto valit = Binary_ns::aliases_only_map.find(value);
                if (valit == Binary_ns::aliases_only_map.end()) {
                    throw UnicodePropertyExpressionError("Erroneous property value for binary property " + property_full_name[theprop]);
                }
                return cast<BinaryPropertyObject>(property_object_table[theprop])->GetCodepointSet(value);
            }
            throw UnicodePropertyExpressionError("Property " + property_full_name[theprop] + " recognized but not supported in icgrep 1.0");
        }
        else {
            // No namespace (property) name.   Try as a general category.
            int valcode = GetPropertyValueEnumCode(gc, value);
            if (valcode >= 0) {
                return cast<EnumeratedPropertyObject>(property_object_table[gc])->GetCodepointSet(valcode);
            }
            valcode = cast<EnumeratedPropertyObject> (property_object_table[sc])->GetPropertyValueEnumCode(value);
            if (valcode >= 0) {
                return cast<EnumeratedPropertyObject>(property_object_table[sc])->GetCodepointSet(valcode);
            }
            // Try as a binary property.
            auto propit = alias_map.find(value);
            if (propit != alias_map.end()) {
                theprop = propit->second;
                if (property_object_table[theprop]->the_kind == PropertyObject::ClassTypeId::BinaryProperty) {
                    return cast<BinaryPropertyObject>(property_object_table[theprop])->GetCodepointSet(valcode);
                }
                else {
                    throw UnicodePropertyExpressionError("Error: property " + property_full_name[theprop] + " specified without a value");
                }
            }
        }
    }
    throw UnicodePropertyExpressionError("Expected a general category, script or binary property name, but '" + name->getName() + "' found instead");
}
