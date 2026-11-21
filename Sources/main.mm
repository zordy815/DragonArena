// lang: ObjC++/C++17 | file: main.mm | runtime: iOS arm64 dylib | target: AB Mobile
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#include "scanner.h"
#include "memory.h"
#include "antiban.h"
#include "overlay.h"

static id<MTLDevice>       g_device      = nil;
static id<MTLCommandQueue> g_queue       = nil;
static bool                g_imgui_ready = false;

static IMP orig_draw_in_view = nullptr;
static void hooked_draw_in_view(id self, SEL _cmd, MTKView* view) {
    ((void(*)(id,SEL,MTKView*))orig_draw_in_view)(self,_cmd,view);
    if(!g_imgui_ready) return;
    id<MTLCommandBuffer> cmd=[g_queue commandBuffer];
    if(!cmd) return;
    id<CAMetalDrawable> drawable=view.currentDrawable;
    if(!drawable) { [cmd commit]; return; }
    MTLRenderPassDescriptor* rp=[MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture=drawable.texture;
    rp.colorAttachments[0].loadAction=MTLLoadActionLoad;
    rp.colorAttachments[0].storeAction=MTLStoreActionStore;
    id<MTLRenderCommandEncoder> enc=[cmd renderCommandEncoderWithDescriptor:rp];
    ImGui_ImplMetal_NewFrame(rp);
    ImGui::NewFrame();
    overlay::update_entities();
    overlay::draw();
    ImGui::Render();
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(),cmd,enc);
    [enc endEncoding];
    [cmd commit];
}

static IMP orig_send_event = nullptr;
static void hooked_send_event(id self, SEL _cmd, UIEvent* event) {
    ((void(*)(id,SEL,UIEvent*))orig_send_event)(self,_cmd,event);
    if(!overlay::cfg.aim_enabled) return;
    auto aim=overlay::get_aim_target();
    if(!aim.valid) return;
    // FILL: hook game's UIPanGestureRecognizer subclass and inject aim.dx/aim.dy as location delta
}

static void install_draw_hook();
static void try_hook(void) {
    UIWindow* win=UIApplication.sharedApplication.windows.firstObject;
    if(!win) { dispatch_after(dispatch_time(DISPATCH_TIME_NOW,500*NSEC_PER_MSEC),
                              dispatch_get_main_queue(),^{try_hook();}); return; }
    __block MTKView* found=nil;
    NSMutableArray* q=[NSMutableArray arrayWithObject:win];
    while(q.count&&!found) {
        UIView* v=q.firstObject; [q removeObjectAtIndex:0];
        if([v isKindOfClass:[MTKView class]]) { found=(MTKView*)v; break; }
        [q addObjectsFromArray:v.subviews];
    }
    if(!found) { dispatch_after(dispatch_time(DISPATCH_TIME_NOW,500*NSEC_PER_MSEC),
                                dispatch_get_main_queue(),^{try_hook();}); return; }

    // screen size from actual view
    CGRect bounds=found.bounds;
    overlay::g_screen_w=(float)bounds.size.width;
    overlay::g_screen_h=(float)bounds.size.height;

    g_device=found.device?:MTLCreateSystemDefaultDevice();
    g_queue=[g_device newCommandQueue];

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io=ImGui::GetIO();
    io.DisplaySize=ImVec2(overlay::g_screen_w,overlay::g_screen_h);
    io.FontGlobalScale=1.5f;
    ImGui::StyleColorsDark();
    ImGui_ImplMetal_Init(g_device);
    g_imgui_ready=true;

    id delegate=found.delegate;
    if(delegate) {
        Class cls=object_getClass(delegate);
        SEL sel=@selector(drawInMTKView:);
        Method m=class_getInstanceMethod(cls,sel);
        if(m) { orig_draw_in_view=method_getImplementation(m);
                method_setImplementation(m,(IMP)hooked_draw_in_view); }
    }

    Class ac=object_getClass(UIApplication.sharedApplication);
    Method sm=class_getInstanceMethod(ac,@selector(sendEvent:));
    if(sm) { orig_send_event=method_getImplementation(sm);
             method_setImplementation(sm,(IMP)hooked_send_event); }

    fprintf(stderr,"[DragonArena] hooks installed. GWorld=%lX\n",mem::get_uworld());
}

__attribute__((constructor))
static void dragon_arena_init() {
    antiban::install();
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,(int64_t)(4.0*NSEC_PER_SEC)),
                   dispatch_get_main_queue(),^{ try_hook(); });
}
