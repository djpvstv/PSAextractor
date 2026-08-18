#include "pac/pac_file.hpp"
#include "psa/misc_section.hpp"

#include <cstdio>
#include <exception>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: psax <path-to-pac>\n");
        return 2;
    }
    try {
        auto pac = psax::PacFile::load(argv[1]);
        std::printf("file: %s\n", argv[1]);
        std::printf("size: %zu bytes\n\n", pac.size());

        std::printf("--- ARC header ---\n");
        std::printf("  name:       %s\n", pac.header().name.c_str());
        std::printf("  version:    0x%04X\n", pac.header().version);
        std::printf("  node_count: %u\n\n", pac.header().node_count);

        std::printf("--- ARC entries ---\n");
        for (std::size_t i = 0; i < pac.entries().size(); ++i) {
            const auto& e = pac.entries()[i];
            std::printf("  [%zu] type=%s (0x%04X) index=%u length=0x%X (%u) group=%u redirect=%d data@0x%zX\n",
                        i, psax::arc_file_type_name(e.file_type),
                        static_cast<uint16_t>(e.file_type),
                        e.file_index, e.length, e.length,
                        e.group_id, e.redirect_index, e.data_offset);
        }

        auto misc = pac.find_misc_data();
        if (!misc) {
            std::printf("\nMiscData: NOT FOUND\n");
            return 0;
        }
        std::printf("\n--- MiscData (PSA section) at file 0x%zX, %u bytes ---\n",
                    misc->data_offset, misc->length);

        psax::MiscSection ms(pac.entry_data(*misc), misc->length);
        const auto& h = ms.header();
        std::printf("  file_size:         %u\n", h.file_size);
        std::printf("  data_table_offset: 0x%X (%u)\n", h.data_table_offset, h.data_table_offset);
        std::printf("  extern_sub_offset: 0x%X (%u)\n", h.extern_sub_offset, h.extern_sub_offset);
        std::printf("  data_table_count:  %u\n", h.data_table_count);
        std::printf("  extern_sub_count:  %u\n", h.extern_sub_count);

        std::printf("\n  --- data table (%zu entries) ---\n", ms.data_table().size());
        for (std::size_t i = 0; i < ms.data_table().size() && i < 20; ++i) {
            const auto& d = ms.data_table()[i];
            try {
                auto name = ms.string_at(d.name_offset);
                std::printf("    [%zu] data@0x%X name@0x%X = \"%.*s\"\n",
                            i, d.data_offset, d.name_offset,
                            static_cast<int>(name.size()), name.data());
            } catch (const std::exception&) {
                std::printf("    [%zu] data@0x%X name@0x%X = <bad string offset>\n",
                            i, d.data_offset, d.name_offset);
            }
        }
        if (ms.data_table().size() > 20) {
            std::printf("    ... and %zu more\n", ms.data_table().size() - 20);
        }

        std::printf("\n  --- external subs (%zu entries) ---\n", ms.external_subs().size());
        for (std::size_t i = 0; i < ms.external_subs().size() && i < 10; ++i) {
            const auto& e = ms.external_subs()[i];
            try {
                auto name = ms.string_at(e.name_offset);
                std::printf("    [%zu] ref@0x%X name@0x%X = \"%.*s\"\n",
                            i, e.data_offset, e.name_offset,
                            static_cast<int>(name.size()), name.data());
            } catch (const std::exception&) {
                std::printf("    [%zu] ref@0x%X name@0x%X = <bad string offset>\n",
                            i, e.data_offset, e.name_offset);
            }
        }
        if (ms.external_subs().size() > 10) {
            std::printf("    ... and %zu more\n", ms.external_subs().size() - 10);
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
