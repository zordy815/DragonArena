// lang: C++17 | file: math.h | runtime: iOS arm64 | target: AB Mobile
#pragma once
#include <cmath>
#include "ue4_types.h"

namespace math {

constexpr float DEG2RAD = 0.017453292519943295f;
constexpr float PI      = 3.14159265358979323846f;

inline FMatrix make_view_matrix(const FMinimalViewInfo& pov, float sw, float sh) {
    float sp=sinf(pov.rotation.pitch*DEG2RAD), cp=cosf(pov.rotation.pitch*DEG2RAD);
    float sy=sinf(pov.rotation.yaw*DEG2RAD),   cy=cosf(pov.rotation.yaw*DEG2RAD);
    float sr=sinf(pov.rotation.roll*DEG2RAD),   cr=cosf(pov.rotation.roll*DEG2RAD);
    FVector ax_x={cp*cy,cp*sy,-sp};
    FVector ax_y={sr*sp*cy-cr*sy, sr*sp*sy+cr*cy, sr*cp};
    FVector ax_z={cr*sp*cy+sr*sy, cr*sp*sy-sr*cy, cr*cp};
    FMatrix m{};
    m.m[0][0]=ax_x.x; m.m[0][1]=ax_y.x; m.m[0][2]=ax_z.x;
    m.m[1][0]=ax_x.y; m.m[1][1]=ax_y.y; m.m[1][2]=ax_z.y;
    m.m[2][0]=ax_x.z; m.m[2][1]=ax_y.z; m.m[2][2]=ax_z.z;
    m.m[3][0]=-(ax_x.x*pov.location.x+ax_x.y*pov.location.y+ax_x.z*pov.location.z);
    m.m[3][1]=-(ax_y.x*pov.location.x+ax_y.y*pov.location.y+ax_y.z*pov.location.z);
    m.m[3][2]=-(ax_z.x*pov.location.x+ax_z.y*pov.location.y+ax_z.z*pov.location.z);
    m.m[3][3]=1.f;
    float half_tan=tanf(pov.fov*DEG2RAD*0.5f), aspect=sw/sh;
    FMatrix proj{};
    proj.m[0][0]=1.f/(aspect*half_tan);
    proj.m[1][1]=1.f/half_tan;
    proj.m[2][3]=1.f;
    proj.m[3][2]=1.f;
    FMatrix vp{};
    for(int r=0;r<4;r++) for(int c=0;c<4;c++) for(int k=0;k<4;k++) vp.m[r][c]+=m.m[r][k]*proj.m[k][c];
    return vp;
}

struct W2SResult { float x, y; bool visible; };
inline W2SResult world_to_screen(const FVector& w, const FMatrix& vp, float sw, float sh) {
    float cx=w.x*vp.m[0][0]+w.y*vp.m[1][0]+w.z*vp.m[2][0]+vp.m[3][0];
    float cy=w.x*vp.m[0][1]+w.y*vp.m[1][1]+w.z*vp.m[2][1]+vp.m[3][1];
    float cw=w.x*vp.m[0][3]+w.y*vp.m[1][3]+w.z*vp.m[2][3]+vp.m[3][3];
    if(cw<0.01f) return {0,0,false};
    float inv=1.f/cw;
    return {sw*0.5f+cx*inv*sw*0.5f, sh*0.5f-cy*inv*sh*0.5f, true};
}

struct AimDelta { float dx, dy; bool in_fov; };
inline AimDelta calc_aim_delta(float tx, float ty, float sw, float sh, float fov_px, float smooth) {
    float dx=tx-sw*0.5f, dy=ty-sh*0.5f;
    return {dx/smooth, dy/smooth, sqrtf(dx*dx+dy*dy)<=fov_px};
}

inline float uu_to_meters(float d) { return d*0.01f; }

} // namespace math
