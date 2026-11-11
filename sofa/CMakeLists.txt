# SOFA (AES69) HRTF file loading: libmysofa (BSD-3-Clause) and zlib, both fetched and built static,
# plus SF2's small loader that turns a file into an sf2::HrirSet.
include(FetchContent)

FetchContent_Declare(zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  do-not-add)   # sources only; built below
FetchContent_Declare(libmysofa
    GIT_REPOSITORY https://github.com/hoene/libmysofa.git
    GIT_TAG        v1.3.5
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  do-not-add)
FetchContent_MakeAvailable(zlib libmysofa)

set(SF2_ZLIB_SOURCES adler32.c crc32.c deflate.c infback.c inffast.c inflate.c inftrees.c
                     trees.c zutil.c compress.c uncompr.c gzclose.c gzlib.c gzread.c gzwrite.c)
list(TRANSFORM SF2_ZLIB_SOURCES PREPEND ${zlib_SOURCE_DIR}/)

set(SF2_MYSOFA_SOURCES hrtf/reader.c hdf/superblock.c hdf/dataobject.c hdf/btree.c hdf/fractalhead.c
                       hdf/gunzip.c hdf/gcol.c hrtf/check.c hrtf/spherical.c hrtf/lookup.c hrtf/tools.c
                       hrtf/kdtree.c hrtf/neighbors.c hrtf/interpolate.c hrtf/resample.c hrtf/loudness.c
                       hrtf/minphase.c hrtf/easy.c hrtf/cache.c resampler/speex_resampler.c)
list(TRANSFORM SF2_MYSOFA_SOURCES PREPEND ${libmysofa_SOURCE_DIR}/src/)

# Headers libmysofa normally generates with CMake.
set(SF2_MYSOFA_GENERATED ${CMAKE_CURRENT_BINARY_DIR}/mysofa-generated)
file(WRITE ${SF2_MYSOFA_GENERATED}/mysofa_export.h "#pragma once\n#define MYSOFA_EXPORT\n")
file(WRITE ${SF2_MYSOFA_GENERATED}/config.h
     "#pragma once\n#define CMAKE_INSTALL_PREFIX \"\"\n#define CPACK_PACKAGE_VERSION_MAJOR 1\n#define CPACK_PACKAGE_VERSION_MINOR 3\n#define CPACK_PACKAGE_VERSION_PATCH 5\n")
if(NOT EXISTS ${zlib_SOURCE_DIR}/zconf.h)
    configure_file(${zlib_SOURCE_DIR}/zconf.h.in ${SF2_MYSOFA_GENERATED}/zconf.h COPYONLY)
endif()

add_library(sf2sofa STATIC ${SF2_ZLIB_SOURCES} ${SF2_MYSOFA_SOURCES} src/SofaFile.cpp)
target_include_directories(sf2sofa
    PUBLIC  include
    PRIVATE ${SF2_MYSOFA_GENERATED} ${zlib_SOURCE_DIR} ${libmysofa_SOURCE_DIR}/src/hrtf)
target_link_libraries(sf2sofa PUBLIC sf2dsp)
set_target_properties(sf2sofa PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)
target_compile_definitions(sf2sofa PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
if(NOT WIN32)
    target_compile_definitions(sf2sofa PRIVATE HAVE_UNISTD_H)
    find_library(SF2_MATH_LIBRARY m)
    if(SF2_MATH_LIBRARY)
        target_link_libraries(sf2sofa PRIVATE ${SF2_MATH_LIBRARY})
    endif()
endif()

# Third-party C code: keep its warnings out of SF2's build output.
set_source_files_properties(${SF2_ZLIB_SOURCES} ${SF2_MYSOFA_SOURCES} PROPERTIES COMPILE_OPTIONS
    "$<IF:$<C_COMPILER_ID:MSVC>,/W0,-w>")

# Path to a measured HRTF set (MIT KEMAR, shipped with libmysofa) for the unit tests.
set(SF2_TEST_SOFA_FILE ${libmysofa_SOURCE_DIR}/share/MIT_KEMAR_normal_pinna.sofa PARENT_SCOPE)
