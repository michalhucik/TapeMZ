cmake_minimum_required(VERSION 3.16)

# MSYS2/MinGW: GCC linker (collect2) vyzaduje zapisovatelny TEMP adresar.
# cc.exe je nativni Windows binary - pouziva systemovy TEMP z Windows registru,
# ktery nemusi byt zapisovatelny. MSYS2 cesta "/tmp" se nepropaguje.
#
# Reseni: buildujte pres "cmake --build build --preset default"
# nebo rucne: TEMP=$(cygpath -w /tmp) TMP=$(cygpath -w /tmp) make -j$(nproc)

project(tmz-format C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Varovani + -pipe (vyhne se TEMP problemum na Windows)
add_compile_options(-Wall -Wextra -Wpedantic -pipe)
add_link_options(-pipe)

# Standalone build - bez baseui z mz800emu
add_compile_definitions(GENERIC_DRIVER_NO_BASEUI)
add_compile_definitions(GENERIC_DRIVER_FILE_CB)
add_compile_definitions(GENERIC_DRIVER_MEMORY_CB)

# Include path - "libs/..." cesty vyzaduji src/ jako koren
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# ===========================================================================
#  Knihovny
# ===========================================================================

# generic_driver
add_library(generic_driver STATIC
    src/libs/generic_driver/generic_driver.c
    src/libs/generic_driver/memory_driver.c
)

# endianity
add_library(endianity STATIC
    src/libs/endianity/endianity.c
)

# sharpmz_ascii
add_library(sharpmz_ascii STATIC
    src/libs/sharpmz_ascii/sharpmz_ascii.c
    src/libs/sharpmz_ascii/sharpmz_utf8.c
)

# wav
add_library(wav STATIC
    src/libs/wav/wav.c
)
target_link_libraries(wav PRIVATE generic_driver endianity)

# mzf
add_library(mzf STATIC
    src/libs/mzf/mzf.c
    src/libs/mzf/mzf_tools.c
)
target_link_libraries(mzf PRIVATE generic_driver endianity sharpmz_ascii)

# cmt_stream
add_library(cmt_stream STATIC
    src/libs/cmt_stream/cmt_bitstream.c
    src/libs/cmt_stream/cmt_vstream.c
    src/libs/cmt_stream/cmt_stream.c
)
target_link_libraries(cmt_stream PRIVATE generic_driver wav)

# cmtspeed
add_library(cmtspeed STATIC
    src/libs/cmtspeed/cmtspeed.c
)

# mztape
add_library(mztape STATIC
    src/libs/mztape/mztape.c
)
target_link_libraries(mztape PRIVATE generic_driver endianity cmt_stream cmtspeed mzf)

# zxtape
add_library(zxtape STATIC
    src/libs/zxtape/zxtape.c
)
target_link_libraries(zxtape PRIVATE cmt_stream cmtspeed mztape)

# tzx
add_library(tzx STATIC
    src/libs/tzx/tzx.c
)
target_link_libraries(tzx PRIVATE generic_driver cmt_stream endianity)

# mzcmt_fsk
add_library(mzcmt_fsk STATIC
    src/libs/mzcmt_fsk/mzcmt_fsk.c
)
target_link_libraries(mzcmt_fsk PRIVATE cmt_stream)

# mzcmt_slow
add_library(mzcmt_slow STATIC
    src/libs/mzcmt_slow/mzcmt_slow.c
)
target_link_libraries(mzcmt_slow PRIVATE cmt_stream)

# mzcmt_direct
add_library(mzcmt_direct STATIC
    src/libs/mzcmt_direct/mzcmt_direct.c
)
target_link_libraries(mzcmt_direct PRIVATE cmt_stream)

# mzcmt_turbo
add_library(mzcmt_turbo STATIC
    src/libs/mzcmt_turbo/mzcmt_turbo.c
)
target_link_libraries(mzcmt_turbo PRIVATE cmt_stream cmtspeed endianity)

# mzcmt_fastipl
add_library(mzcmt_fastipl STATIC
    src/libs/mzcmt_fastipl/mzcmt_fastipl.c
)
target_link_libraries(mzcmt_fastipl PRIVATE cmt_stream cmtspeed endianity)

# mzcmt_bsd
add_library(mzcmt_bsd STATIC
    src/libs/mzcmt_bsd/mzcmt_bsd.c
)
target_link_libraries(mzcmt_bsd PRIVATE cmt_stream cmtspeed endianity)

# mzcmt_cpmtape
add_library(mzcmt_cpmtape STATIC
    src/libs/mzcmt_cpmtape/mzcmt_cpmtape.c
)
target_link_libraries(mzcmt_cpmtape PRIVATE cmt_stream endianity)

# wav_analyzer
add_library(wav_analyzer STATIC
    src/libs/wav_analyzer/wav_analyzer_types.c
    src/libs/wav_analyzer/wav_preprocess.c
    src/libs/wav_analyzer/wav_pulse.c
    src/libs/wav_analyzer/wav_leader.c
    src/libs/wav_analyzer/wav_histogram.c
    src/libs/wav_analyzer/wav_classify.c
    src/libs/wav_analyzer/wav_decode_fm.c
    src/libs/wav_analyzer/wav_decode_turbo.c
    src/libs/wav_analyzer/wav_decode_fastipl.c
    src/libs/wav_analyzer/wav_decode_bsd.c
    src/libs/wav_analyzer/wav_decode_cpmtape.c
    src/libs/wav_analyzer/wav_decode_fsk.c
    src/libs/wav_analyzer/wav_decode_slow.c
    src/libs/wav_analyzer/wav_decode_direct.c
    src/libs/wav_analyzer/wav_decode_zx.c
    src/libs/wav_analyzer/wav_analyzer.c
)
target_link_libraries(wav_analyzer PRIVATE generic_driver wav mzf endianity sharpmz_ascii)

# tmz
add_library(tmz STATIC
    src/libs/tmz/tmz.c
    src/libs/tmz/tmz_blocks.c
    src/libs/tmz/tmz_player.c
)
target_link_libraries(tmz PRIVATE endianity mzf mztape zxtape tzx cmt_stream cmtspeed
    mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_cpmtape)

# ===========================================================================
#  Utility
# ===========================================================================

# tmzinfo
add_executable(tmzinfo
    src/tools/tmzinfo/tmzinfo.c
)
target_link_libraries(tmzinfo PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_cpmtape)

# mzf2tmz
add_executable(mzf2tmz
    src/tools/mzf2tmz/mzf2tmz.c
)
target_link_libraries(mzf2tmz PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_cpmtape)

# tmz2mzf
add_executable(tmz2mzf
    src/tools/tmz2mzf/tmz2mzf.c
)
target_link_libraries(tmz2mzf PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_cpmtape)

# tmz2wav
add_executable(tmz2wav
    src/tools/tmz2wav/tmz2wav.c
)
target_link_libraries(tmz2wav PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_cpmtape)

# tap2tmz
add_executable(tap2tmz
    src/tools/tap2tmz/tap2tmz.c
)
target_link_libraries(tap2tmz PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_cpmtape)

# tmz2tap
add_executable(tmz2tap
    src/tools/tmz2tap/tmz2tap.c
)
target_link_libraries(tmz2tap PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_cpmtape)

# tmzconv
add_executable(tmzconv
    src/tools/tmzconv/tmzconv.c
)
target_link_libraries(tmzconv PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_cpmtape)

# tmzedit
add_executable(tmzedit
    src/tools/tmzedit/tmzedit.c
)
target_link_libraries(tmzedit PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_cpmtape)

# wav2tmz
add_executable(wav2tmz
    src/tools/wav2tmz/wav2tmz.c
)
target_link_libraries(wav2tmz PRIVATE wav_analyzer tmz tzx mzf sharpmz_ascii generic_driver endianity wav cmtspeed)

# dat2bsd
add_executable(dat2bsd
    src/tools/dat2bsd/dat2bsd.c
)
target_link_libraries(dat2bsd PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_cpmtape)

# bsd2dat
add_executable(bsd2dat
    src/tools/bsd2dat/bsd2dat.c
)
target_link_libraries(bsd2dat PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_cpmtape)

# Linkovani math knihovny (pro round(), floor() atd.)
target_link_libraries(mztape PRIVATE m)
target_link_libraries(tzx PRIVATE m)
target_link_libraries(mzcmt_fsk PRIVATE m)
target_link_libraries(mzcmt_slow PRIVATE m)
target_link_libraries(mzcmt_direct PRIVATE m)
target_link_libraries(mzcmt_turbo PRIVATE m)
target_link_libraries(mzcmt_fastipl PRIVATE m)
target_link_libraries(mzcmt_bsd PRIVATE m)
target_link_libraries(mzcmt_cpmtape PRIVATE m)
target_link_libraries(wav_analyzer PRIVATE m)

# ===========================================================================
#  Testy
# ===========================================================================

# test_cpm_roundtrip
add_executable(test_cpm_roundtrip
    tests/test_cpm_roundtrip.c
)
target_link_libraries(test_cpm_roundtrip PRIVATE wav_analyzer mztape mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_mz80b_roundtrip
add_executable(test_mz80b_roundtrip
    tests/test_mz80b_roundtrip.c
)
target_link_libraries(test_mz80b_roundtrip PRIVATE wav_analyzer mztape mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_cpmtape_roundtrip
add_executable(test_cpmtape_roundtrip
    tests/test_cpmtape_roundtrip.c
)
target_link_libraries(test_cpmtape_roundtrip PRIVATE wav_analyzer mzcmt_cpmtape mzf sharpmz_ascii generic_driver endianity wav cmt_stream m)

# test_fsk_roundtrip
add_executable(test_fsk_roundtrip
    tests/test_fsk_roundtrip.c
)
target_link_libraries(test_fsk_roundtrip PRIVATE wav_analyzer mzcmt_fsk mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_slow_roundtrip
add_executable(test_slow_roundtrip
    tests/test_slow_roundtrip.c
)
target_link_libraries(test_slow_roundtrip PRIVATE wav_analyzer mzcmt_slow mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_direct_roundtrip
add_executable(test_direct_roundtrip
    tests/test_direct_roundtrip.c
)
target_link_libraries(test_direct_roundtrip PRIVATE wav_analyzer mzcmt_direct mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_fsk_tape_roundtrip (dvourychlostni FSK tape signal)
add_executable(test_fsk_tape_roundtrip
    tests/test_fsk_tape_roundtrip.c
)
target_link_libraries(test_fsk_tape_roundtrip PRIVATE wav_analyzer mzcmt_fsk mzcmt_turbo mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_slow_tape_roundtrip (dvourychlostni SLOW tape signal)
add_executable(test_slow_tape_roundtrip
    tests/test_slow_tape_roundtrip.c
)
target_link_libraries(test_slow_tape_roundtrip PRIVATE wav_analyzer mzcmt_slow mzcmt_turbo mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_direct_tape_roundtrip (dvourychlostni DIRECT tape signal)
add_executable(test_direct_tape_roundtrip
    tests/test_direct_tape_roundtrip.c
)
target_link_libraries(test_direct_tape_roundtrip PRIVATE wav_analyzer mzcmt_direct mzcmt_turbo mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_zx_roundtrip
add_executable(test_zx_roundtrip
    tests/test_zx_roundtrip.c
)
target_link_libraries(test_zx_roundtrip PRIVATE wav_analyzer zxtape mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed mztape m)

# test_ic_normal - integracni test realne nahravky Intercopy NORMAL
add_executable(test_ic_normal
    tests/test_ic_normal.c
)
target_link_libraries(test_ic_normal PRIVATE wav_analyzer mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed mztape m)

# test_ic_wav - genericky integracni test pro libovolny WAV
add_executable(test_ic_wav
    tests/test_ic_wav.c
)
target_link_libraries(test_ic_wav PRIVATE wav_analyzer mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed mztape m)

# test_tc_normal - integracni test TurboCopy NORMAL nahravek (vsechny rychlosti)
add_executable(test_tc_normal
    tests/test_tc_normal.c
)
target_link_libraries(test_tc_normal PRIVATE wav_analyzer mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# Adresar pro docasne soubory testu (WAV vystup roundtrip testu)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/temp)
