// at top of overlay.h, before namespace overlay {
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_metal.h"
#include "memory.h"
#include "ue4_math.h"
#include "ue4_types.h"

namespace overlay {
// ... rest of file, no imgui includes inside namespace

struct Config {
    bool  esp_enabled   = true;
    bool  esp_boxes     = true;
    bool  esp_bones     = true;
    bool  esp_health    = true;
    bool  esp_names     = true;
    bool  esp_distance  = true;
    bool  esp_lines     = false;
    float esp_max_range = 300.f;
    bool  aim_enabled   = true;
    float aim_fov       = 80.f;
    float aim_smooth    = 6.f;
    int   aim_bone      = offsets::bone::HEAD;
};
static Config cfg;

struct Entity {
    uintptr_t actor, mesh;
    FVector   head_ws, root_ws;
    float     distance_m;
    float     health, health_max;
    std::string name;
    math::W2SResult head_ss, root_ss, feet_ss;
    math::W2SResult bones_ss[14];
};

static std::vector<Entity> g_entities;
static float g_screen_w=390.f, g_screen_h=844.f;
static FCameraCacheEntry g_cam;
static FMatrix g_vp;

static std::string read_fstring(uintptr_t addr) {
    struct FString { wchar_t* data; int32_t count; int32_t max; };
    FString fs = mem::read<FString>(addr);
    if(!fs.data||fs.count<=0||fs.count>64) return "";
    std::string out; out.reserve(fs.count);
    for(int i=0;i<fs.count-1;i++)
        out+=(char)(mem::read<wchar_t>((uintptr_t)fs.data+i*sizeof(wchar_t))&0x7F);
    return out;
}

static float g_last_hmax = 100.f;
static std::pair<float,float> read_health(uintptr_t actor) {
    uintptr_t hc = mem::read<uintptr_t>(actor + offsets::APAWN_HEALTH_COMP);
    if(!hc) return {-1.f,-1.f};
    float cur = mem::read<float>(hc + offsets::HEALTH_COMP_CURRENT);
    float max = mem::read<float>(hc + offsets::HEALTH_COMP_MAX);
    if(cur<0.f||cur>10000.f) return {-1.f,-1.f};
    if(max>10.f&&max<10000.f) g_last_hmax=max;
    return {cur, g_last_hmax};
}

inline void update_entities() {
    g_entities.clear();
    g_cam = mem::get_camera_cache();
    g_vp  = math::make_view_matrix(g_cam.pov, g_screen_w, g_screen_h);
    uintptr_t local_pawn = mem::get_local_pawn();
    TArray<uintptr_t> actors = mem::get_actors();
    if(!actors.valid()) return;

    for(int i=0;i<actors.count;i++) {
        uintptr_t actor = actors[i];
        if(!actor||actor==local_pawn) continue;
        uintptr_t mesh = mem::read<uintptr_t>(actor + offsets::APAWN_MESH);
        if(!mesh) continue;
        uintptr_t rc = mem::read<uintptr_t>(actor + offsets::AACTOR_ROOT_COMPONENT);
        if(!rc) continue;
        FVector root_ws = mem::read<FVector>(rc + offsets::USCENE_RELATIVE_LOCATION);
        FVector head_ws = mem::get_bone_pos(mesh, offsets::bone::HEAD);
        if(head_ws.x==0.f&&head_ws.y==0.f&&head_ws.z==0.f) continue;
        float dist_m = math::uu_to_meters(g_cam.pov.location.dist_to(root_ws));
        if(dist_m > cfg.esp_max_range) continue;

        Entity e{};
        e.actor=actor; e.mesh=mesh;
        e.head_ws=head_ws; e.root_ws=root_ws;
        e.distance_m=dist_m;
        auto [hp,hmax] = read_health(actor);
        e.health=hp; e.health_max=hmax;

        uintptr_t ps = mem::read<uintptr_t>(actor + offsets::APAWN_PLAYER_STATE);
        if(ps) e.name = read_fstring(ps + offsets::PLAYERSTATE_PLAYER_NAME);

        e.head_ss = math::world_to_screen(head_ws, g_vp, g_screen_w, g_screen_h);
        e.root_ss = math::world_to_screen(root_ws, g_vp, g_screen_w, g_screen_h);
        FVector feet_ws=root_ws; feet_ws.z-=90.f;
        e.feet_ss = math::world_to_screen(feet_ws, g_vp, g_screen_w, g_screen_h);

        static const int bone_ids[14]={
            offsets::bone::HEAD,  offsets::bone::NECK,      offsets::bone::CHEST,
            offsets::bone::PELVIS,offsets::bone::L_SHOULDER,offsets::bone::R_SHOULDER,
            offsets::bone::L_ELBOW,offsets::bone::R_ELBOW,  offsets::bone::L_WRIST,
            offsets::bone::R_WRIST,offsets::bone::L_KNEE,   offsets::bone::R_KNEE,
            offsets::bone::L_FOOT, offsets::bone::R_FOOT
        };
        for(int b=0;b<14;b++) {
            FVector bw=mem::get_bone_pos(mesh,bone_ids[b]);
            e.bones_ss[b]=math::world_to_screen(bw,g_vp,g_screen_w,g_screen_h);
        }
        g_entities.push_back(e);
    }
}

struct AimTarget { float dx, dy; bool valid; };
inline AimTarget get_aim_target() {
    if(!cfg.aim_enabled||g_entities.empty()) return {0,0,false};
    float cx=g_screen_w*0.5f, cy=g_screen_h*0.5f, best=cfg.aim_fov;
    AimTarget res{0,0,false};
    for(auto& e:g_entities) {
        auto& ss=e.bones_ss[0];
        if(!ss.visible) continue;
        float dx=ss.x-cx, dy=ss.y-cy, d=sqrtf(dx*dx+dy*dy);
        if(d<best) { best=d; res={dx/cfg.aim_smooth,dy/cfg.aim_smooth,true}; }
    }
    return res;
}

inline void draw() {
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::SetNextWindowPos({0,0});
    ImGui::SetNextWindowSize({g_screen_w,g_screen_h});
    ImGui::Begin("##esp",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|
        ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImDrawList* dl=ImGui::GetWindowDrawList();

    for(auto& e:g_entities) {
        if(!e.head_ss.visible||!e.feet_ss.visible) continue;
        float h=e.feet_ss.y-e.head_ss.y;
        if(h<4.f) continue;
        float w=h*0.4f, ex=e.head_ss.x, ey=e.head_ss.y;

        if(cfg.esp_boxes) {
            dl->AddRect({ex-w-1,ey-1},{ex+w+1,e.feet_ss.y+1},IM_COL32(0,0,0,180),0,0,2.5f);
            dl->AddRect({ex-w,ey},{ex+w,e.feet_ss.y},IM_COL32(255,255,255,210),0,0,1.5f);
        }

        if(cfg.esp_health&&e.health>=0.f) {
            float frac=(e.health_max>0.f)?e.health/e.health_max:0.f;
            float bx=ex-w-5.f, bt=ey, bb=e.feet_ss.y;
            float fy=bb-(bb-bt)*frac;
            dl->AddRectFilled({bx-1,bt-1},{bx+3,bb+1},IM_COL32(0,0,0,180));
            dl->AddRectFilled({bx,fy},{bx+2,bb},
                IM_COL32((uint8_t)((1.f-frac)*255),(uint8_t)(frac*255),0,220));
        }

        if(cfg.esp_bones) {
            static const int pairs[][2]={
                {0,1},{1,2},{2,3},
                {2,4},{4,6},{6,8},
                {2,5},{5,7},{7,9},
                {3,10},{10,12},{3,11},{11,13}
            };
            for(auto& p:pairs) {
                auto& a=e.bones_ss[p[0]], &b=e.bones_ss[p[1]];
                if(!a.visible||!b.visible) continue;
                dl->AddLine({a.x,a.y},{b.x,b.y},IM_COL32(0,255,0,180),1.2f);
            }
        }

        if(cfg.esp_names&&!e.name.empty())
            dl->AddText({ex-w,ey-26.f},IM_COL32(255,255,255,255),e.name.c_str());

        if(cfg.esp_distance) {
            char buf[16]; snprintf(buf,sizeof(buf),"%.0fm",e.distance_m);
            dl->AddText({ex-w,ey-14.f},IM_COL32(200,200,200,220),buf);
        }

        if(cfg.esp_lines)
            dl->AddLine({g_screen_w*0.5f,g_screen_h},{e.root_ss.x,e.root_ss.y},
                        IM_COL32(255,0,0,100),1.f);
    }

    if(cfg.aim_enabled)
        dl->AddCircle({g_screen_w*0.5f,g_screen_h*0.5f},
                      cfg.aim_fov,IM_COL32(255,255,255,70),64,1.f);

    ImGui::End();
}

} // namespace overlay
