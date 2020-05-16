# main

import string, os.path
from unihan_parser import *

def emit_enumerated_property(f, property_code, independent_prop_values, prop_values, value_map):
    f.write("  namespace %s_ns {\n" % property_code.upper())
    f.write("    const unsigned independent_prop_values = %s;\n" % independent_prop_values)
    for v in prop_values:
        f.write("    /** Code Point Ranges for %s\n    " % v)
        f.write(cformat.multiline_fill(['[%04x, %04x]' % (lo, hi) for (lo, hi) in uset_to_range_list(value_map[v])], ',', 4))
        f.write("**/\n\n")
        f.write(value_map[v].generate(v.lower() + "_Set", 4))
    set_list = ['&%s_Set' % v.lower() for v in prop_values]
    f.write("    static EnumeratedPropertyObject property_object\n")
    f.write("        {%s,\n" % property_code)
    f.write("        %s_ns::independent_prop_values,\n" % property_code.upper())
    f.write("        std::move(%s_ns::enum_names),\n" % property_code.upper())
    f.write("        std::move(%s_ns::value_names),\n" % property_code.upper())
    f.write("        std::move(%s_ns::aliases_only_map),{\n" % property_code.upper())
    f.write("        " + cformat.multiline_fill(set_list, ',', 8))
    f.write("\n        }};"
            "\n    }\n")

def sum_bytes(value_map):
    sum = 0
    for i in value_map.keys():
        sum += value_map[i].bytes()
    return sum

def get_property_full_name(property_code):
    name = None
    if property_code == "kpy":
        name = "KHanyuPinyin"
    return name 
class unihan_generator():
    def __init__(self):
        self.parsed_map = []
        self.supported_props = []
        self.property_data_headers = []

    def emit_property(self, f, property_code, prop_values, independent_prop_values, value_map):
        full_name = get_property_full_name(property_code)
        emit_enumerated_property(f, property_code, independent_prop_values, prop_values, value_map)
        print("%s: %s bytes" % (full_name, sum_bytes(value_map)))
        self.supported_props.append(property_code)

    def generate_property_value_file(self, filename_root, property_code):
        prop_values, independent_prop_values, value_map = parse_property_file(filename_root, property_code)
        prop_name = get_property_full_name(property_code)
        f = cformat.open_header_file_for_write(prop_name)
        cformat.write_imports(f, ['"PropertyAliases.h"', '"PropertyObjects.h"', '"PropertyValueAliases.h"', '"unicode_set.h"'])
        f.write("\nnamespace UCD {\n")
        self.emit_property(f, property_code, prop_values, independent_prop_values, value_map)
        f.write("}\n")
        cformat.close_header_file(f)
        self.property_data_headers.append(prop_name)

def unihan_main():
    unihan = unihan_generator()
    unihan.generate_property_value_file('Unihan_Readings', 'kpy')

if __name__ == "__main__":
  unihan_main()
