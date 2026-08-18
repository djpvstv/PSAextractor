#include "util/binary_reader.hpp"

#include <cstring>
#include <fstream>
#include <stdexcept>

namespace psax {

BinaryReader::BinaryReader(const uint8_t* data, size_t size)
    : data_(data), size_(size) {}

static void check_bounds(size_t cursor, size_t need, size_t size) {
    if (cursor + need > size) {
        throw std::out_of_range("BinaryReader: read past end of buffer");
    }
}

uint8_t BinaryReader::read_u8() {
    check_bounds(cursor_, 1, size_);
    return data_[cursor_++];
}

uint16_t BinaryReader::read_u16_be() {
    check_bounds(cursor_, 2, size_);
    uint16_t v = static_cast<uint16_t>((uint16_t(data_[cursor_]) << 8) |
                                        uint16_t(data_[cursor_ + 1]));
    cursor_ += 2;
    return v;
}

uint32_t BinaryReader::read_u32_be() {
    check_bounds(cursor_, 4, size_);
    uint32_t v = (uint32_t(data_[cursor_])     << 24) |
                 (uint32_t(data_[cursor_ + 1]) << 16) |
                 (uint32_t(data_[cursor_ + 2]) <<  8) |
                 (uint32_t(data_[cursor_ + 3]));
    cursor_ += 4;
    return v;
}

int32_t BinaryReader::read_i32_be() {
    return static_cast<int32_t>(read_u32_be());
}

float BinaryReader::read_f32_be() {
    uint32_t bits = read_u32_be();
    float f;
    std::memcpy(&f, &bits, sizeof(float));
    return f;
}

uint32_t BinaryReader::peek_u32_be() const {
    check_bounds(cursor_, 4, size_);
    return (uint32_t(data_[cursor_])     << 24) |
           (uint32_t(data_[cursor_ + 1]) << 16) |
           (uint32_t(data_[cursor_ + 2]) <<  8) |
           (uint32_t(data_[cursor_ + 3]));
}

std::string_view BinaryReader::read_cstring_at(size_t offset) const {
    if (offset >= size_) {
        throw std::out_of_range("BinaryReader: cstring offset past end");
    }
    const char* start = reinterpret_cast<const char*>(data_ + offset);
    size_t max_len = size_ - offset;
    size_t len = 0;
    while (len < max_len && start[len] != '\0') ++len;
    return std::string_view(start, len);
}

void BinaryReader::seek(size_t offset) {
    if (offset > size_) {
        throw std::out_of_range("BinaryReader: seek past end");
    }
    cursor_ = offset;
}

std::vector<uint8_t> load_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("cannot open file: " + path);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    if (size > 0 && !f.read(reinterpret_cast<char*>(buf.data()), size)) {
        throw std::runtime_error("read failed: " + path);
    }
    return buf;
}

} // namespace psax
