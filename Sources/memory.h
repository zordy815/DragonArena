// lang: C++17 | file: memory.h | runtime: iOS arm64 dylib in-process | target: AB Mobile
#pragma once
#include <cstdint>
#include <cstring>
#include "ue4_types.h"
#include "scanner.h"

namespace mem {

template<typename T>
inline T read(uintptr_t addr) {
    if (!addr) return T{};
    T val{};
    memcpy(&val, (void*)addr, sizeof(T));
    return val;
}

inline uintptr_t* find_gworld() {
    static uintptr_t* cached = nullptr;
    if (cached) return cached;
    auto range = scanner::get_main_image_range();
    // confirmed from UAGame Nov 2026 binary -- 206 refs, resolves to DATA:0x10D857378
    const char pattern[] = "\x28\x7A\x04\xD0\x08\xBD\x41\xF9";
    const char mask[]    = "????x??x";
    uint8_t* hit = scanner::pattern_scan(range.text_start, range.text_end - range.text_start, pattern, mask);
    if (!hit) return nullptr;
    uintptr_t ptr_addr = scanner::resolve_adrp_ldr((uintptr_t)hit);
    if (!ptr_addr) return nullptr;
    cached = (uintptr_t*)ptr_addr;
    return cached;
}

inline uintptr_t get_uworld() {
    uintptr_t* gw = find_gworld();
    return gw ? *gw : 0;
}

inline uintptr_t get_persistent_level() {
    uintptr_t w = get_uworld();
    return w ? read<uintptr_t>(w + offsets::UWORLD_PERSISTENT_LEVEL) : 0;
}

inline TArray<uintptr_t> get_actors() {
    uintptr_t level = get_persistent_level();
    if (!level) return {};
    return read<TArray<uintptr_t>>(level + offsets::ULEVEL_ACTORS);
}

inline uintptr_t get_local_playercontroller() {
    uintptr_t world = get_uworld();
    if (!world) return 0;
    uintptr_t gi = read<uintptr_t>(world + offsets::UWORLD_OWNING_GAMEINSTANCE);
    if (!gi) return 0;
    TArray<uintptr_t> players = read<TArray<uintptr_t>>(gi + offsets::GAMEINSTANCE_LOCAL_PLAYERS);
    if (!players.valid()) return 0;
    uintptr_t lp = players[0];
    return lp ? read<uintptr_t>(lp + offsets::LOCALPLAYER_PLAYERCONTROLLER) : 0;
}

inline uintptr_t get_camera_manager() {
    uintptr_t pc = get_local_playercontroller();
    return pc ? read<uintptr_t>(pc + offsets::PLAYERCONTROLLER_CAMERA_MGR) : 0;
}

inline FCameraCacheEntry get_camera_cache() {
    uintptr_t cm = get_camera_manager();
    return cm ? read<FCameraCacheEntry>(cm + offsets::CAMERA_CACHE) : FCameraCacheEntry{};
}

inline uintptr_t get_local_pawn() {
    uintptr_t pc = get_local_playercontroller();
    return pc ? read<uintptr_t>(pc + offsets::PLAYERCONTROLLER_PAWN) : 0;
}

inline FVector get_bone_pos(uintptr_t mesh, int32_t bone_idx) {
    if (!mesh) return {};
    TArray<uint8_t> bones = read<TArray<uint8_t>>(mesh + offsets::MESH_BONE_ARRAY);
    if (!bones.valid()) return {};
    uintptr_t transform_addr = (uintptr_t)bones.data + (bone_idx * 48);
    return read<FVector>(transform_addr + 16);
}

} // namespace mem
