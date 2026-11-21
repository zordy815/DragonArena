// lang: C++17 | file: ue4_types.h | runtime: iOS arm64 | target: Arena Breakout Mobile (UAGame)
#pragma once
#include <cstdint>
#include <cmath>

struct FVector {
    float x, y, z;
    float dist_to(const FVector& o) const {
        float dx=x-o.x, dy=y-o.y, dz=z-o.z;
        return sqrtf(dx*dx+dy*dy+dz*dz);
    }
};
struct FVector2D { float x, y; };
struct FQuat     { float x, y, z, w; };
struct FRotator  { float pitch, yaw, roll; };

struct FMinimalViewInfo {
    FVector  location;
    FRotator rotation;
    float    fov;
    uint8_t  _pad[0x24];
};

struct FCameraCacheEntry {
    float            timestamp;
    uint8_t          _pad[0x0C];
    FMinimalViewInfo pov;
};

struct FMatrix { float m[4][4]; };

template<typename T>
struct TArray {
    T*      data;
    int32_t count;
    int32_t max;
    T& operator[](int i) const { return data[i]; }
    bool valid() const { return data && count > 0 && count < 0x2000; }
};

struct FName { int32_t comparison_index; int32_t number; };

struct UObject {
    void*   vtable;
    int32_t object_flags;
    int32_t internal_idx;
    void*   uclass;
    FName   name;
    void*   outer;
};

namespace offsets {
    constexpr uintptr_t UWORLD_PERSISTENT_LEVEL     = 0x0030;
    constexpr uintptr_t UWORLD_OWNING_GAMEINSTANCE  = 0x0180;
    constexpr uintptr_t ULEVEL_ACTORS               = 0x0098;
    constexpr uintptr_t AACTOR_ROOT_COMPONENT       = 0x0130;
    constexpr uintptr_t USCENE_RELATIVE_LOCATION    = 0x011C;
    constexpr uintptr_t APAWN_MESH                  = 0x0280;
    constexpr uintptr_t MESH_BONE_ARRAY             = 0x0520;
    constexpr uintptr_t PLAYERCONTROLLER_PAWN       = 0x0278;
    constexpr uintptr_t PLAYERCONTROLLER_CAMERA_MGR = 0x0438;
    constexpr uintptr_t CAMERA_CACHE               = 0x0440;
    constexpr uintptr_t GAMEINSTANCE_LOCAL_PLAYERS  = 0x0038;
    constexpr uintptr_t LOCALPLAYER_PLAYERCONTROLLER= 0x0030;
    constexpr uintptr_t PLAYERSTATE_PLAYER_NAME     = 0x0308;
    constexpr uintptr_t APAWN_PLAYER_STATE          = 0x02A8;
    constexpr uintptr_t APAWN_HEALTH_COMP           = 0x03A8;
    constexpr uintptr_t HEALTH_COMP_CURRENT         = 0x0238;
    constexpr uintptr_t HEALTH_COMP_MAX             = 0x023C;

    enum bone : int32_t {
        HEAD=7, NECK=6, CHEST=5, PELVIS=0,
        L_SHOULDER=11, R_SHOULDER=36,
        L_ELBOW=12,    R_ELBOW=37,
        L_WRIST=13,    R_WRIST=38,
        L_KNEE=61,     R_KNEE=56,
        L_FOOT=63,     R_FOOT=58,
    };
}
