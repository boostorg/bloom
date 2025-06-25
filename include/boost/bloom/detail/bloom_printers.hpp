// Copyright 2025 Braden Ganetsky
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Generated on 2025-06-25T14:45:13

#ifndef BOOST_BLOOM_DETAIL_BLOOM_PRINTERS_HPP
#define BOOST_BLOOM_DETAIL_BLOOM_PRINTERS_HPP

#ifndef BOOST_ALL_NO_EMBEDDED_GDB_SCRIPTS
#if defined(__ELF__)
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverlength-strings"
#endif
__asm__(".pushsection \".debug_gdb_scripts\", \"MS\",%progbits,1\n"
        ".ascii \"\\4gdb.inlined-script.BOOST_BLOOM_DETAIL_BLOOM_PRINTERS_HPP\\n\"\n"
        ".ascii \"import gdb.printing\\n\"\n"

        ".ascii \"class BoostBloomFilterPrinter:\\n\"\n"
        ".ascii \"    def __init__(self, val):\\n\"\n"
        ".ascii \"        self.void_pointer = gdb.lookup_type(\\\"void\\\").pointer()\\n\"\n"
        ".ascii \"        nullptr = gdb.Value(0).cast(self.void_pointer)\\n\"\n"

        ".ascii \"        has_array = val[\\\"ar\\\"][\\\"data\\\"] != nullptr\\n\"\n"

        ".ascii \"        if has_array:\\n\"\n"
        ".ascii \"            stride = int(val[\\\"stride\\\"])\\n\"\n"
        ".ascii \"            used_value_size = int(val[\\\"used_value_size\\\"])\\n\"\n"
        ".ascii \"            self.array_size = int(val[\\\"hs\\\"][\\\"rng\\\"]) * stride + (used_value_size - stride)\\n\"\n"
        ".ascii \"        else:\\n\"\n"
        ".ascii \"            self.array_size = 0\\n\"\n"
        ".ascii \"        self.capacity = self.array_size * 8\\n\"\n"
        ".ascii \"        if has_array:\\n\"\n"
        ".ascii \"            self.data = val[\\\"ar\\\"][\\\"array\\\"]\\n\"\n"
        ".ascii \"        else:\\n\"\n"
        ".ascii \"            self.data = nullptr\\n\"\n"

        ".ascii \"    def to_string(self):\\n\"\n"
        ".ascii \"        return f\\\"boost::bloom::filter with {{capacity = {self.capacity}, data = {self.data.cast(self.void_pointer)}, size = {self.array_size}}}\\\"\\n\"\n"

        ".ascii \"    def display_hint(self):\\n\"\n"
        ".ascii \"        return \\\"map\\\"\\n\"\n"
        ".ascii \"    def children(self):\\n\"\n"
        ".ascii \"        def generator():\\n\"\n"
        ".ascii \"            data = self.data\\n\"\n"
        ".ascii \"            for i in range(self.array_size):\\n\"\n"
        ".ascii \"                yield \\\"\\\", f\\\"{i}\\\"\\n\"\n"
        ".ascii \"                yield \\\"\\\", data.dereference()\\n\"\n"
        ".ascii \"                data = data + 1\\n\"\n"
        ".ascii \"        return generator()\\n\"\n"

        ".ascii \"def boost_bloom_build_pretty_printer():\\n\"\n"
        ".ascii \"    pp = gdb.printing.RegexpCollectionPrettyPrinter(\\\"boost_bloom\\\")\\n\"\n"
        ".ascii \"    add_template_printer = lambda name, printer: pp.add_printer(name, f\\\"^{name}<.*>$\\\", printer)\\n\"\n"

        ".ascii \"    add_template_printer(\\\"boost::bloom::filter\\\", BoostBloomFilterPrinter)\\n\"\n"

        ".ascii \"    return pp\\n\"\n"

        ".ascii \"gdb.printing.register_pretty_printer(gdb.current_objfile(), boost_bloom_build_pretty_printer())\\n\"\n"

        ".byte 0\n"
        ".popsection\n");
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif // defined(__ELF__)
#endif // !defined(BOOST_ALL_NO_EMBEDDED_GDB_SCRIPTS)

#endif // !defined(BOOST_BLOOM_DETAIL_BLOOM_PRINTERS_HPP)
