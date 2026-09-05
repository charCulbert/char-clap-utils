#pragma once

#include <clap/clap.h>

#include <cstddef>
#include <cstdint>

namespace char_clap
{

inline bool writeComplete(const clap_ostream_t& stream, const void* data, size_t size) noexcept
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    size_t offset = 0;
    while (offset < size)
    {
        const auto written = stream.write(&stream, bytes + offset, size - offset);
        if (written <= 0 || static_cast<uint64_t>(written) > size - offset) return false;
        offset += static_cast<size_t>(written);
    }
    return true;
}

inline bool readComplete(const clap_istream_t& stream, void* data, size_t size) noexcept
{
    auto* bytes = static_cast<uint8_t*>(data);
    size_t offset = 0;
    while (offset < size)
    {
        const auto read = stream.read(&stream, bytes + offset, size - offset);
        if (read <= 0 || static_cast<uint64_t>(read) > size - offset) return false;
        offset += static_cast<size_t>(read);
    }
    return true;
}

} // namespace char_clap
