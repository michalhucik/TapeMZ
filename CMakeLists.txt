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
add_compile_options(-Wall -Wextra -Wpedantic -Wjump-misses-init -pipe)
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

# mzcmt_manchester (generátor rodiny MANCHESTER pro blok 0x46)
add_library(mzcmt_manchester STATIC
    src/libs/mzcmt_manchester/mzcmt_manchester.c
)
target_link_libraries(mzcmt_manchester PRIVATE cmt_stream)

# mzcmt_fm (generátor FM pro blok 0x46)
add_library(mzcmt_fm STATIC
    src/libs/mzcmt_fm/mzcmt_fm.c
)
target_link_libraries(mzcmt_fm PRIVATE cmt_stream)

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
target_link_libraries(wav_analyzer PRIVATE generic_driver wav mzf endianity sharpmz_ascii cmtspeed)

# tmz
add_library(tmz STATIC
    src/libs/tmz/tmz.c
    src/libs/tmz/tmz_blocks.c
    src/libs/tmz/tmz_player.c
    src/libs/tmz/tmz_tlv.c
    src/libs/tmz/tmz_mzfile.c
    src/libs/tmz/tmz_part_fm.c
    src/libs/tmz/tmz_part_manchester.c
    src/libs/tmz/tmz_profile.c
    src/libs/tmz/tmz_v1conv.c
    src/libs/tmz/tmz_mzt.c
)
target_link_libraries(tmz PRIVATE endianity mzf mztape zxtape tzx cmt_stream cmtspeed
    mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester)

# ===========================================================================
#  Utility
# ===========================================================================

# tmzinfo
add_executable(tmzinfo
    src/tools/tmzinfo/tmzinfo.c
)
target_link_libraries(tmzinfo PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd)

# mzf2tmz
add_executable(mzf2tmz
    src/tools/mzf2tmz/mzf2tmz.c
)
target_link_libraries(mzf2tmz PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd)

# tmz2mzf
add_executable(tmz2mzf
    src/tools/tmz2mzf/tmz2mzf.c
)
target_link_libraries(tmz2mzf PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd)

# tmz2wav
add_executable(tmz2wav
    src/tools/tmz2wav/tmz2wav.c
)
target_link_libraries(tmz2wav PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd)

# tap2tmz
add_executable(tap2tmz
    src/tools/tap2tmz/tap2tmz.c
)
target_link_libraries(tap2tmz PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd)

# tmz2tap
add_executable(tmz2tap
    src/tools/tmz2tap/tmz2tap.c
)
target_link_libraries(tmz2tap PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd)

# tmzconv
add_executable(tmzconv
    src/tools/tmzconv/tmzconv.c
)
target_link_libraries(tmzconv PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd)

# tmzedit
add_executable(tmzedit
    src/tools/tmzedit/tmzedit.c
)
target_link_libraries(tmzedit PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd)

# wav2tmz
add_executable(wav2tmz
    src/tools/wav2tmz/wav2tmz.c
)
target_link_libraries(wav2tmz PRIVATE wav_analyzer tmz tzx mzf sharpmz_ascii generic_driver endianity wav cmtspeed)

# dat2bsd
add_executable(dat2bsd
    src/tools/dat2bsd/dat2bsd.c
)
target_link_libraries(dat2bsd PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd)

# bsd2dat
add_executable(bsd2dat
    src/tools/bsd2dat/bsd2dat.c
)
target_link_libraries(bsd2dat PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd)

# extract_preloader - extraktor TurboCopy TURBO preloader binarky z WAV
add_executable(extract_preloader
    src/tools/extract_preloader/extract_preloader.c
)
target_link_libraries(extract_preloader PRIVATE wav_analyzer mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# Linkovani math knihovny (pro round(), floor() atd.)
target_link_libraries(mztape PRIVATE m)
target_link_libraries(tzx PRIVATE m)
target_link_libraries(mzcmt_fsk PRIVATE m)
target_link_libraries(mzcmt_slow PRIVATE m)
target_link_libraries(mzcmt_direct PRIVATE m)
target_link_libraries(mzcmt_turbo PRIVATE m)
target_link_libraries(mzcmt_fastipl PRIVATE m)
target_link_libraries(mzcmt_bsd PRIVATE m)
target_link_libraries(wav_analyzer PRIVATE m)

# ===========================================================================
#  Testy
# ===========================================================================

# test_cpm_roundtrip
add_executable(test_cpm_roundtrip
    tests/test_cpm_roundtrip.c
)
target_link_libraries(test_cpm_roundtrip PRIVATE wav_analyzer mztape mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_ic_turbo_files (datový leader každého souboru v ic-turbo.wav)
add_executable(test_ic_turbo_files
    tests/test_ic_turbo_files.c
)
target_link_libraries(test_ic_turbo_files PRIVATE wav_analyzer mzf sharpmz_ascii generic_driver endianity wav cmtspeed m)

# test_fm_copy2 (druhá kopie ROM záznamu)
add_executable(test_fm_copy2
    tests/test_fm_copy2.c
)
target_link_libraries(test_fm_copy2 PRIVATE wav_analyzer mzf sharpmz_ascii generic_driver endianity wav cmtspeed m)

# test_fm_recovery (obnova hlavičky a těla FM z poškozené sekvence)
add_executable(test_fm_recovery
    tests/test_fm_recovery.c
)
target_link_libraries(test_fm_recovery PRIVATE wav_analyzer mzf sharpmz_ascii generic_driver endianity wav cmtspeed m)

# test_wav_formats (WAV EXTENSIBLE a IEEE float)
add_executable(test_wav_formats
    tests/test_wav_formats.c
)
target_link_libraries(test_wav_formats PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav wav_analyzer mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_fm_header_only (hlavička s fsize=0 bez těla)
add_executable(test_fm_header_only
    tests/test_fm_header_only.c
)
target_link_libraries(test_fm_header_only PRIVATE wav_analyzer mzf sharpmz_ascii generic_driver endianity wav cmtspeed m)

# test_tmz_blocks_parse (validace velikosti dat v blocích 0x40-0x44)
add_executable(test_tmz_blocks_parse
    tests/test_tmz_blocks_parse.c
)
target_link_libraries(test_tmz_blocks_parse PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd m)

# test_tzx_load (načítání TZX, useknutí, 0x35, CSW hranice)
add_executable(test_tzx_load
    tests/test_tzx_load.c
)
target_link_libraries(test_tzx_load PRIVATE tzx cmt_stream generic_driver endianity wav cmtspeed m)

# test_turbo_readpoint (TURBO pulzy vs čtecí bod ROM)
add_executable(test_turbo_readpoint
    tests/test_turbo_readpoint.c
)
target_link_libraries(test_turbo_readpoint PRIVATE mzcmt_turbo cmt_stream mzf sharpmz_ascii generic_driver endianity wav cmtspeed m)

# test_turbo_roundtrip (TURBO kodér -> WAV -> wav_analyzer)
add_executable(test_turbo_roundtrip
    tests/test_turbo_roundtrip.c
)
target_link_libraries(test_turbo_roundtrip PRIVATE wav_analyzer mzcmt_turbo cmt_stream mzf sharpmz_ascii generic_driver endianity wav cmtspeed m)

# test_tmz_player_turbo (blok 0x41 TURBO s vlastními pulzy přes player)
add_executable(test_tmz_player_turbo
    tests/test_tmz_player_turbo.c
)
target_link_libraries(test_tmz_player_turbo PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd m)

# test_tmz_tlv (TLV pro blok 0x46)
add_executable(test_tmz_tlv
    tests/test_tmz_tlv.c
)
target_link_libraries(test_tmz_tlv PRIVATE tmz)

# test_tmz_mzfile (blok 0x46 MZ File)
add_executable(test_tmz_mzfile
    tests/test_tmz_mzfile.c
)
target_link_libraries(test_tmz_mzfile PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd m)

# test_tmz_fuzz (náhodné mutace bloku 0x46)
add_executable(test_tmz_fuzz
    tests/test_tmz_fuzz.c
)
target_link_libraries(test_tmz_fuzz PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_tzx_level (úroveň signálu a pilot generátorů TZX)
add_executable(test_tzx_level
    tests/test_tzx_level.c
)
target_link_libraries(test_tzx_level PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_tmz_player_level (úroveň signálu, zastavení pásky, rezervované bloky v playeru)
add_executable(test_tmz_player_level
    tests/test_tmz_player_level.c
)
target_link_libraries(test_tmz_player_level PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_mzcmt_manchester (generátor MANCHESTER)
add_executable(test_mzcmt_manchester
    tests/test_mzcmt_manchester.c
)
target_link_libraries(test_mzcmt_manchester PRIVATE mzcmt_manchester cmt_stream wav generic_driver endianity m)

# test_tmz_part_manchester (TLV části MANCHESTER)
add_executable(test_tmz_part_manchester
    tests/test_tmz_part_manchester.c
)
target_link_libraries(test_tmz_part_manchester PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_mzcmt_fm (generátor FM)
add_executable(test_mzcmt_fm
    tests/test_mzcmt_fm.c
)
target_link_libraries(test_mzcmt_fm PRIVATE mzcmt_fm cmt_stream wav generic_driver endianity m)

# test_tmz_part_fm (TLV části FM <-> parametry)
add_executable(test_tmz_part_fm
    tests/test_tmz_part_fm.c
)
target_link_libraries(test_tmz_part_fm PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_tmz_v2_ref (vektory proti záznamům z emulátoru)
add_executable(test_tmz_v2_ref
    tests/test_tmz_v2_ref.c
)
target_link_libraries(test_tmz_v2_ref PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_tmz_profile (registr profilů proti záznamům z emulátoru)
add_executable(test_tmz_profile
    tests/test_tmz_profile.c
)
target_link_libraries(test_tmz_profile PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_tmz_player_mzfile (player a blok 0x46)
add_executable(test_tmz_player_mzfile
    tests/test_tmz_player_mzfile.c
)
target_link_libraries(test_tmz_player_mzfile PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_tmz_v1legacy (profil TAPEMZ-V1-LEGACY proti generátorům v1.0)
add_executable(test_tmz_v1legacy
    tests/test_tmz_v1legacy.c
)
target_link_libraries(test_tmz_v1legacy PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_mzf_terminator (bajt 11h hlavičky MZF beze změny)
add_executable(test_mzf_terminator
    tests/test_mzf_terminator.c
)
target_link_libraries(test_mzf_terminator PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_tmz_v1conv (převod bloků v1.0 na 0x46)
add_executable(test_tmz_v1conv
    tests/test_tmz_v1conv.c
)
target_link_libraries(test_tmz_v1conv PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_tmz_mzt (MZT a hlavička CMTSPEED)
add_executable(test_tmz_mzt
    tests/test_tmz_mzt.c
)
target_link_libraries(test_tmz_mzt PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_mz80b_roundtrip
add_executable(test_mz80b_roundtrip
    tests/test_mz80b_roundtrip.c
)
target_link_libraries(test_mz80b_roundtrip PRIVATE wav_analyzer mztape mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_cpmtape_roundtrip (CPM-TAPE přes profily PEZIK-2.0A a IC-10.2-CPM)
add_executable(test_cpmtape_roundtrip
    tests/test_cpmtape_roundtrip.c
)
target_link_libraries(test_cpmtape_roundtrip PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav wav_analyzer mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

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

# test_tape_body_crc (kontrolní součet těla FSK/SLOW/DIRECT ve wav_analyzer)
add_executable(test_tape_body_crc
    tests/test_tape_body_crc.c
)
target_link_libraries(test_tape_body_crc PRIVATE wav_analyzer mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_turbo mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_direct_tape_wav (WAV round-trip: MZF -> DIRECT tape WAV -> wav_analyzer -> MZF)
add_executable(test_direct_tape_wav
    tests/test_direct_tape_wav.c
)
target_link_libraries(test_direct_tape_wav PRIVATE wav_analyzer mzcmt_direct mzcmt_turbo mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_bsd_wav (BSD integrační test: nahrávka z emulátoru + WAV round-trip + TMZ round-trip)
add_executable(test_bsd_wav
    tests/test_bsd_wav.c
)
target_link_libraries(test_bsd_wav PRIVATE tmz mzf sharpmz_ascii generic_driver endianity cmt_stream wav wav_analyzer mztape zxtape tzx cmtspeed mzcmt_turbo mzcmt_fastipl mzcmt_fsk mzcmt_slow mzcmt_direct mzcmt_bsd mzcmt_fm mzcmt_manchester m)

# test_bsd_incomplete (BSD recovery test: nekompletni nahravka s/bez --recover-bsd)
add_executable(test_bsd_incomplete
    tests/test_bsd_incomplete.c
)
target_link_libraries(test_bsd_incomplete PRIVATE wav_analyzer tmz tzx mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed mztape m)

# test_zx_roundtrip
add_executable(test_zx_roundtrip
    tests/test_zx_roundtrip.c
)
target_link_libraries(test_zx_roundtrip PRIVATE wav_analyzer zxtape mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed mztape m)

# test_ic_normal - integrační test nahrávky Intercopy NORMAL z emulátoru
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

# test_tc_loader_all - integracni test TurboCopy TURBO loader nahravky (5x Turbo Copy V1.21)
add_executable(test_tc_loader_all
    tests/test_tc_loader_all.c
)
target_link_libraries(test_tc_loader_all PRIVATE wav_analyzer mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_ic_loader_all - integracni test Intercopy FAST IPL loader nahravky (4x Turbo Copy V1.21)
add_executable(test_ic_loader_all
    tests/test_ic_loader_all.c
)
target_link_libraries(test_ic_loader_all PRIVATE wav_analyzer mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# test_ic_sinclair_all - integracni test Intercopy SINCLAIR nahravky (4x Turbo Copy V1.21)
add_executable(test_ic_sinclair_all
    tests/test_ic_sinclair_all.c
)
target_link_libraries(test_ic_sinclair_all PRIVATE wav_analyzer mzf sharpmz_ascii generic_driver endianity wav cmt_stream cmtspeed m)

# Adresar pro docasne soubory testu (WAV vystup roundtrip testu)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/temp)
