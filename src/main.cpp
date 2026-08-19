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
        std::printf("  file_size (+0x00): %u\n", h.file_size);
        std::printf("  word1     (+0x04): 0x%X (%u)\n", h.word1, h.word1);
        std::printf("  word2     (+0x08): 0x%X (%u)\n", h.word2, h.word2);
        std::printf("  word3     (+0x0C): %u\n", h.word3);
        std::printf("  word4     (+0x10): %u\n", h.word4);
        std::printf("  (bytes 0x14..0x1F confirmed zero)\n");

        std::printf("\n  --- derived table layout ---\n");
        std::printf("  string_pool_start:       0x%zX\n", ms.string_pool_start());
        std::printf("  external_sub_table_start:0x%zX\n", ms.external_sub_table_start());
        std::printf("  data_table_start:        0x%zX\n", ms.data_table_start());

        std::printf("\n  --- data table (%zu entries) ---\n", ms.data_table().size());
        for (std::size_t i = 0; i < ms.data_table().size() && i < 5; ++i) {
            const auto& d = ms.data_table()[i];
            try {
                auto n = ms.name_at(d.name_rel);
                std::printf("    [%zu] name_rel=0x%X data_ref=0x%X name=\"%.*s\"\n",
                            i, d.name_rel, d.data_ref,
                            static_cast<int>(n.size()), n.data());
            } catch (const std::exception& ex) {
                std::printf("    [%zu] name_rel=0x%X data_ref=0x%X ERROR: %s\n",
                            i, d.name_rel, d.data_ref, ex.what());
            }
        }
        std::printf("\n  --- external subs (%zu entries, first 10) ---\n", ms.external_subs().size());
        for (std::size_t i = 0; i < ms.external_subs().size() && i < 10; ++i) {
            const auto& e = ms.external_subs()[i];
            try {
                auto n = ms.name_at(e.name_rel);
                std::printf("    [%zu] name_rel=0x%X data_ref=0x%X name=\"%.*s\"\n",
                            i, e.name_rel, e.data_ref,
                            static_cast<int>(n.size()), n.data());
            } catch (const std::exception& ex) {
                std::printf("    [%zu] name_rel=0x%X data_ref=0x%X ERROR: %s\n",
                            i, e.name_rel, e.data_ref, ex.what());
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
