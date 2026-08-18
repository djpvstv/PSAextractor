#include <ostream>
#include <stdexcept>
#include <string>

#include <doctest/doctest.h>

#include "util/binary_reader.hpp"

TEST_CASE("BinaryReader reads big-endian primitives") {
    const uint8_t bytes[] = {
        0x12, 0x34, 0x56, 0x78,   // u32 = 0x12345678
        0xFF, 0xFE,               // u16 = 0xFFFE
        0xAB,                     // u8  = 0xAB
        0x40, 0x49, 0x0F, 0xDB,   // f32 ~= pi
    };
    psax::BinaryReader r(bytes, sizeof(bytes));

    CHECK(r.read_u32_be() == 0x12345678u);
    CHECK(r.read_u16_be() == 0xFFFEu);
    CHECK(r.read_u8() == 0xABu);
    CHECK(r.read_f32_be() == doctest::Approx(3.14159274f));
    CHECK(r.eof());
}

TEST_CASE("BinaryReader bounds-checks short reads") {
    const uint8_t bytes[] = { 0x01, 0x02 };
    psax::BinaryReader r(bytes, sizeof(bytes));
    CHECK_THROWS_AS(r.read_u32_be(), std::out_of_range);
}

TEST_CASE("BinaryReader reads NUL-terminated string at offset") {
    const uint8_t bytes[] = { 'A','B','C', 0, 'D','E', 0 };
    psax::BinaryReader r(bytes, sizeof(bytes));
    CHECK(r.read_cstring_at(0) == "ABC");
    CHECK(r.read_cstring_at(4) == "DE");
}

TEST_CASE("BinaryReader seek + tell round-trip") {
    const uint8_t bytes[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    psax::BinaryReader r(bytes, sizeof(bytes));
    r.seek(2);
    CHECK(r.tell() == 2u);
    CHECK(r.read_u8() == 0xCCu);
}
