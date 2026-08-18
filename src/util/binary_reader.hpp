#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace psax {

// Big-endian reader over a byte buffer. PAC files use PowerPC/Wii byte order,
// so every multi-byte primitive is big-endian on disk.
class BinaryReader {
public:
    BinaryReader(const uint8_t* data, size_t size);

    uint8_t  read_u8();
    uint16_t read_u16_be();
    uint32_t read_u32_be();
    int32_t  read_i32_be();
    float    read_f32_be();

    uint32_t peek_u32_be() const;

    // NUL-terminated string at an absolute offset; does not move the cursor.
    std::string_view read_cstring_at(std::size_t offset) const;

    void seek(size_t offset);
    size_t tell() const { return cursor_; }
    size_t size() const { return size_; }
    const uint8_t* data() const { return data_; }
    bool eof() const { return cursor_ >= size_; }

private:
    const uint8_t* data_;
    size_t size_;
    size_t cursor_ = 0;
};

std::vector<uint8_t> load_file(const std::string& path);

} // namespace psax
