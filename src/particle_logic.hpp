#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ats_exhaust {
// Windows ATS 1.61.3.18 only. Evidence: compatibility/windows-1.61.3.18.md.
inline constexpr char game_sha256[] = "c60e8ee02cc97b3cc6ef8c6298bb5c3c23c4ffd330b8185b646fb1b608a3c7b4";
inline constexpr std::uintptr_t allocator_rva = 0x1f1520;
inline constexpr std::uintptr_t caller_return_rva = 0x1584428;
inline constexpr std::uintptr_t generator_vtable_rva = 0x2435af0;
inline constexpr std::uintptr_t container_vtable_rva = 0x2435ad8;
inline constexpr std::size_t container_offset = 0x650;
inline constexpr std::array<unsigned char,16> allocator_bytes = {
    0x48,0x83,0xc1,0x28,0xe9,0xf7,0x0c,0x00,0x00,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc};
inline constexpr char allocator_signature[] = "48 83 C1 28 E9 F7 0C 00 00 CC CC CC CC CC CC CC";
struct ParticleFields {
    std::uintptr_t generator_vtable = 0;
    std::uintptr_t container_vtable = 0;
    std::uint32_t count = 0;
    float size_x = 0;
    float size_y = 0;
    float lifetime = 0;
};
inline bool near(float a, float b) { return std::isfinite(a) && std::fabs(a-b) <= 0.0005F; }
inline int classify(const ParticleFields& f, std::uintptr_t base) {
    if (!base || f.generator_vtable != base + generator_vtable_rva ||
        f.container_vtable != base + container_vtable_rva) return -1;
    if (f.count==40 && near(f.size_x,0.090F) && near(f.size_y,0.090F) && near(f.lifetime,3.0F)) return 0;
    if (f.count==20 && near(f.size_x,0.040F) && near(f.size_y,0.040F) && near(f.lifetime,0.7F)) return 1;
    if (f.count==4 && near(f.size_x,0.020F) && near(f.size_y,0.020F) && near(f.lifetime,0.2F)) return 2;
    // Dynamic-blend presets need different band weights. Leave those generators unchanged.
    return -1;
}
template<class T> inline T field(const unsigned char* p, std::size_t offset) {
    T value{}; std::memcpy(&value,p+offset,sizeof(T)); return value;
}
inline ParticleFields read_fields(const unsigned char* generator) {
    return {field<std::uintptr_t>(generator,0), field<std::uintptr_t>(generator,container_offset),
            field<std::uint32_t>(generator,0x638), field<float>(generator,0x54),
            field<float>(generator,0x58), field<float>(generator,0x634)};
}
inline bool eligible(const ParticleFields& f, std::uintptr_t base, std::uintptr_t caller) {
    return caller == base + caller_return_rva && classify(f,base) >= 0;
}
inline bool admit(float demand, std::uint32_t ordinal) {
    if (!std::isfinite(demand) || demand <= 0.0F) return false;
    if (demand >= 1.0F) return true;
    const auto sample = (ordinal * 2654435761U) >> 8U;
    return sample < static_cast<std::uint32_t>(demand * 16777216.0F);
}
}
