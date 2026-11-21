// lang: C++17 | file: scanner.h | runtime: iOS arm64 dylib | target: AB Mobile
#pragma once
#include <cstdint>
#include <cstddef>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <string>
#include <vector>

namespace scanner {

struct image_range { uintptr_t base=0, text_start=0, text_end=0, data_start=0, data_end=0; };

inline image_range get_main_image_range() {
    image_range r{};
    uint32_t count = _dyld_image_count();
    for (uint32_t i = 0; i < count; i++) {
        const mach_header_64* hdr = (const mach_header_64*)_dyld_get_image_header(i);
        if (!hdr || hdr->filetype != MH_EXECUTE) continue;
        r.base = (uintptr_t)hdr;
        uintptr_t cur = (uintptr_t)hdr + sizeof(mach_header_64);
        for (uint32_t c = 0; c < hdr->ncmds; c++) {
            segment_command_64* seg = (segment_command_64*)cur;
            if (seg->cmd == LC_SEGMENT_64) {
                if (strcmp(seg->segname,"__TEXT")==0) {
                    r.text_start = r.base + seg->vmaddr;
                    r.text_end   = r.text_start + seg->vmsize;
                } else if (strcmp(seg->segname,"__DATA")==0 || strcmp(seg->segname,"__DATA_CONST")==0) {
                    if (!r.data_start) r.data_start = r.base + seg->vmaddr;
                    r.data_end = r.base + seg->vmaddr + seg->vmsize;
                }
            }
            cur += seg->cmdsize;
        }
        break;
    }
    return r;
}

inline uint8_t* pattern_scan(uintptr_t start, size_t size, const char* pattern, const char* mask) {
    size_t len = strlen(mask);
    uint8_t* base = (uint8_t*)start;
    for (size_t i = 0; i + len < size; i++) {
        bool found = true;
        for (size_t j = 0; j < len; j++)
            if (mask[j]=='x' && base[i+j]!=(uint8_t)pattern[j]) { found=false; break; }
        if (found) return base+i;
    }
    return nullptr;
}

inline uintptr_t resolve_adrp_ldr(uintptr_t adrp_addr) {
    uint32_t adrp = *(uint32_t*)adrp_addr;
    uint32_t ldr  = *(uint32_t*)(adrp_addr+4);
    if ((adrp & 0x9F000000) != 0x90000000) return 0;
    int32_t immlo = (adrp>>29)&0x3;
    int32_t immhi = (adrp>>5)&0x7FFFF;
    int64_t imm   = ((int64_t)((immhi<<2)|immlo)<<43)>>31;
    uintptr_t page = (adrp_addr & ~0xFFFULL) + imm;
    if ((ldr & 0xFFC00000) != 0xF9400000) return page;
    uint32_t imm12 = (ldr>>10)&0xFFF;
    return page + (imm12*8);
}

} // namespace scanner
