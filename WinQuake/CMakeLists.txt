cmake_minimum_required(VERSION 3.15)
project(Quake_ASCII C) 

# --- Global Output Directory Setup ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# ============================================================================
# MODERN NO-OP FPU REPLACEMENTS
# ============================================================================
set(FPU_STUB_FILE "${CMAKE_BINARY_DIR}/fpu_stubs.c")
file(WRITE ${FPU_STUB_FILE} "
#include <stdint.h>
uint16_t ceil_cw = 0x1cbc;
uint16_t single_cw = 0x137f;
uint16_t full_cw = 0x137f;
")

# ============================================================================
# PURE C ENGINE SOURCE FILES
# ============================================================================
# Notice that QUAKE_ASM_SOURCES is completely gone. We are only compiling C.
set(QUAKE_CORE_SOURCES
    cl_demo.c     cl_input.c    cl_main.c     cl_parse.c    cl_tent.c
    chase.c       cmd.c         common.c      console.c     crc.c
    cvar.c        draw.c        d_edge.c      d_fill.c      d_init.c
    d_modech.c    d_part.c      d_polyse.c    d_scan.c      d_sky.c
    d_sprite.c    d_surf.c      d_vars.c      d_zpoint.c    host.c
    host_cmd.c    keys.c        menu.c        mathlib.c     model.c
    net_dgrm.c    net_loop.c    net_main.c    net_vcr.c     net_udp.c
    net_bsd.c     nonintel.c    pr_cmds.c     pr_edict.c    pr_exec.c
    r_aclip.c     r_alias.c     r_bsp.c       r_light.c     r_draw.c
    r_efrag.c     r_edge.c      r_misc.c      r_main.c      r_sky.c
    r_sprite.c    r_surf.c      r_part.c      r_vars.c      screen.c
    sbar.c        sv_main.c     sv_phys.c     sv_move.c     sv_user.c
    zone.c        view.c        wad.c         world.c       cd_null.c
    sys_null2.c   vid_x.c       sx/scr.c      snd_dma.c     snd_mem.c
    snd_mix.c snd_null2.c
    ${FPU_STUB_FILE}
)

set(EXEC_NAME "asciiquake")
add_executable(quake ${QUAKE_CORE_SOURCES})

# --- Include Header Lookups ---
target_include_directories(quake PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/include/common
    ${CMAKE_SOURCE_DIR}/include/reactphysics3d/include
    ${CMAKE_SOURCE_DIR}/include/GameNetworkingSockets/include
    ${CMAKE_SOURCE_DIR}/include/SDL/include
    ${CMAKE_SOURCE_DIR} 
)

# --- Compile Definitions ---
target_compile_definitions(quake PRIVATE  
    stricmp=strcasecmp
    ELF
    id386=0
# Add these to force the renderer to use C instead of ASM
    USE_ASM=0 
    __i386__=0 
)

# --- Base C Flags (Platform Independent) ---
target_compile_options(quake PRIVATE 
    -O3
    -g
    -fcommon
    -std=gnu99
    -Wall 
    -Wextra
    -Dstricmp=strcasecmp
    -Uid386
)

# --- Architecture & Platform Specific Setup ---
if(WIN32)
    # Wymagane makro dla winnt.h wskazujące architekturę i386
    target_compile_definitions(quake PRIVATE _X86_=1)
    
    # Podstawowe biblioteki Windows (zamiast linuksowego libm i libdl)
    target_link_libraries(quake PRIVATE wsock32 winmm)
else()
    # Flagi dedykowane wyłącznie dla natywnej kompilacji na Linux x86
    target_compile_options(quake PRIVATE
        -m32
        -U__i386__
        -U__i386
    )
    target_link_options(quake PRIVATE -m32 -no-pie)
    target_link_libraries(quake PRIVATE m ${CMAKE_DL_LIBS})
endif()

set_target_properties(quake PROPERTIES 
    OUTPUT_NAME ${EXEC_NAME}
)