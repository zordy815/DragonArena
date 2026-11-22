// lang: C++17 | file: antiban.h | runtime: iOS arm64 dylib | target: AB Mobile
// defeats: anogs telemetry DNS, codesign integrity callback, dyld image name scan
// caught by: infoArrayCount slot still present, server replay analysis
#pragma once
#include <dlfcn.h>
#include <netdb.h>
#include <cstring>
#include <cstdio>
#include <mach/mach.h>
#include <mach/task_info.h>
#include <mach/mach_traps.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

namespace antiban {

static const char* const BLOCKED[] = {
    "listdl.com","glcs.listdl.com","anogs.listdl.com",nullptr
};

static int  (*orig_getaddrinfo)(const char*,const char*,const struct addrinfo*,struct addrinfo**) = nullptr;
static bool (*orig_mis_validate)(void*,void*) = nullptr;

static int hooked_getaddrinfo(const char* h,const char* s,const struct addrinfo* hi,struct addrinfo** r) {
    if(h) for(int i=0;BLOCKED[i];i++) if(strstr(h,BLOCKED[i])) return EAI_NONAME;
    return orig_getaddrinfo(h,s,hi,r);
}
static bool hooked_mis_validate(void*,void*) { return true; }

static void rw_write_ptr(uintptr_t* slot, uintptr_t val) {
    // CoW-remap page rw, write, restore r--
    // defeats: __DATA_CONST write fault
    uintptr_t page = (uintptr_t)slot & ~(uintptr_t)0x3FF;
    vm_protect(mach_task_self(), page, 0x400, false,
               VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
    *slot = val;
    vm_protect(mach_task_self(), page, 0x400, false, VM_PROT_READ);
}

static void rebind_symbol(const char* sym, void* rep, void** orig) {
    for(uint32_t img=0;img<_dyld_image_count();img++) {
        const mach_header_64* hdr=(const mach_header_64*)_dyld_get_image_header(img);
        if(!hdr||hdr->filetype!=MH_EXECUTE) continue;
        intptr_t slide=_dyld_get_image_vmaddr_slide(img);
        uintptr_t cur=(uintptr_t)hdr+sizeof(mach_header_64);
        const symtab_command*   sc=nullptr;
        const dysymtab_command* dc=nullptr;
        const uint8_t*          li=nullptr;
        for(uint32_t c=0;c<hdr->ncmds;c++){
            load_command* lc=(load_command*)cur;
            if(lc->cmd==LC_SYMTAB)        sc=(symtab_command*)lc;
            else if(lc->cmd==LC_DYSYMTAB) dc=(dysymtab_command*)lc;
            else if(lc->cmd==LC_SEGMENT_64){
                segment_command_64* sg=(segment_command_64*)lc;
                if(strcmp(sg->segname,"__LINKEDIT")==0)
                    li=(uint8_t*)(slide+sg->vmaddr-sg->fileoff);
            }
            cur+=lc->cmdsize;
        }
        if(!sc||!dc||!li) break;
        const nlist_64* symtab=(const nlist_64*)(li+sc->symoff);
        const char*     strtab=(const char*)(li+sc->stroff);
        const uint32_t* indir =(const uint32_t*)(li+dc->indirectsymoff);
        cur=(uintptr_t)hdr+sizeof(mach_header_64);
        for(uint32_t c=0;c<hdr->ncmds;c++){
            load_command* lc=(load_command*)cur;
            if(lc->cmd==LC_SEGMENT_64){
                segment_command_64* sg=(segment_command_64*)lc;
                section_64* sect=(section_64*)((uintptr_t)sg+sizeof(segment_command_64));
                for(uint32_t s=0;s<sg->nsects;s++,sect++){
                    uint32_t t=sect->flags&SECTION_TYPE;
                    if(t!=S_LAZY_SYMBOL_POINTERS&&t!=S_NON_LAZY_SYMBOL_POINTERS) continue;
                    uint32_t   np  =(uint32_t)(sect->size/sizeof(uintptr_t));
                    uintptr_t* ptrs=(uintptr_t*)(slide+sect->addr);
                    for(uint32_t p=0;p<np;p++){
                        uint32_t si=indir[sect->reserved1+p];
                        if(si==INDIRECT_SYMBOL_ABS||si==INDIRECT_SYMBOL_LOCAL||si>=sc->nsyms) continue;
                        const char* n=strtab+symtab[si].n_un.n_strx;
                        if(n[0]=='_') n++;
                        if(strcmp(n,sym)==0){
                            if(orig&&!*orig) *orig=(void*)ptrs[p];
                            rw_write_ptr(&ptrs[p],(uintptr_t)rep);
                        }
                    }
                }
            }
            cur+=lc->cmdsize;
        }
        break;
    }
}

static void hide_from_dyld(const char* dylib_substr) {
    // must only be called after dyld has fully initialized infoArray
    // defeats: dylib name scan via dyld_all_image_infos
    // caught by: infoArrayCount slot still present
    task_dyld_info_data_t info{};
    mach_msg_type_number_t cnt = TASK_DYLD_INFO_COUNT;
    if(task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&info, &cnt) != KERN_SUCCESS)
        return;
    auto* all = (dyld_all_image_infos*)info.all_image_info_addr;
    if(!all || all->version < 1 || !all->infoArray || !all->infoArrayCount) return;
    auto* arr = (dyld_image_info*)all->infoArray;
    for(uint32_t i = 0; i < all->infoArrayCount; i++) {
        const char* path = arr[i].imageFilePath;
        if(!path) continue;
        if(strstr(path, dylib_substr)) {
            uintptr_t page = (uintptr_t)&arr[i] & ~(uintptr_t)0x3FF;
            vm_protect(mach_task_self(), page, 0x400, false,
                       VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
            arr[i].imageFilePath    = "";
            arr[i].imageLoadAddress = nullptr;
            vm_protect(mach_task_self(), page, 0x400, false, VM_PROT_READ);
            break;
        }
    }
}

static void on_image_added(const mach_header* mh, intptr_t) {
    // fires per-image after dyld fully maps it — infoArray is safe to touch here
    for(uint32_t i = 0; i < _dyld_image_count(); i++) {
        if((const mach_header*)_dyld_get_image_header(i) == mh) {
            const char* name = _dyld_get_image_name(i);
            if(name && strstr(name, "DragonArena"))
                hide_from_dyld("DragonArena");
            break;
        }
    }
}

inline void install() {
    // register callback — fires after dyld fully maps each image, safe to touch infoArray
    _dyld_register_func_for_add_image(on_image_added);
    rebind_symbol("getaddrinfo",         (void*)hooked_getaddrinfo,  (void**)&orig_getaddrinfo);
    rebind_symbol("MISValidateSignature",(void*)hooked_mis_validate, (void**)&orig_mis_validate);
    fprintf(stderr,"[DragonArena] antiban installed\n");
}

} // namespace antiban
