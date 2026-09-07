cmake_minimum_required(VERSION 3.25)
project(PalResourceFactoryNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

foreach(required UE4SS_SOURCE_DIR UE4SS_IMPORT_LIBRARY EXTERNAL_ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

add_library(UE4SSRuntime SHARED IMPORTED GLOBAL)
set_target_properties(UE4SSRuntime PROPERTIES IMPORTED_IMPLIB "${UE4SS_IMPORT_LIBRARY}")

add_library(PalResourceFactoryNative SHARED
    src/PalResourceFactoryNative.cpp
    ../src/core/FacilityState.cpp
)

target_include_directories(PalResourceFactoryNative PRIVATE
    ../src/core
    "${UE4SS_SOURCE_DIR}/UE4SS/include"
    "${UE4SS_SOURCE_DIR}/UE4SS/generated_include"
    "${UE4SS_SOURCE_DIR}/deps/first/Unreal/include"
    "${UE4SS_SOURCE_DIR}/deps/first/Unreal/include/Unreal"
    "${UE4SS_SOURCE_DIR}/deps/first/Unreal/include/Unreal/Core"
    "${UE4SS_SOURCE_DIR}/deps/first/Unreal/generated_include"
    "${UE4SS_SOURCE_DIR}/deps/first/LuaMadeSimple/include"
    "${UE4SS_SOURCE_DIR}/deps/first/LuaRaw/include"
    "${UE4SS_SOURCE_DIR}/deps/first/String/include"
    "${UE4SS_SOURCE_DIR}/deps/first/File/include"
    "${UE4SS_SOURCE_DIR}/deps/first/Function/include"
    "${UE4SS_SOURCE_DIR}/deps/first/Helpers/include"
    "${UE4SS_SOURCE_DIR}/deps/first/Input/include"
    "${UE4SS_SOURCE_DIR}/deps/first/IniParser/include"
    "${UE4SS_SOURCE_DIR}/deps/first/JSON/include"
    "${UE4SS_SOURCE_DIR}/deps/first/MProgram/include"
    "${UE4SS_SOURCE_DIR}/deps/first/ParserBase/include"
    "${UE4SS_SOURCE_DIR}/deps/first/Profiler/include"
    "${UE4SS_SOURCE_DIR}/deps/first/ScopedTimer/include"
    "${UE4SS_SOURCE_DIR}/deps/first/SinglePassSigScanner/include"
    "${UE4SS_SOURCE_DIR}/deps/first/Constructs/include"
    "${UE4SS_SOURCE_DIR}/deps/first/DynamicOutput/include"
    "${UE4SS_SOURCE_DIR}/deps/first/ASMHelper/include"
    "${EXTERNAL_ROOT}/fmt/include"
    "${EXTERNAL_ROOT}/zydis/include"
    "${EXTERNAL_ROOT}/zycore/include"
    "${EXTERNAL_ROOT}/imgui"
    "${EXTERNAL_ROOT}/imgui-text-edit"
    "${EXTERNAL_ROOT}/icon-font-cpp-headers"
)

target_compile_definitions(PalResourceFactoryNative PRIVATE
    UE_BUILD_SHIPPING=1
    UE_GAME=1
    PLATFORM_WINDOWS=1
    PLATFORM_MICROSOFT=1
    OVERRIDE_PLATFORM_HEADER_NAME=Windows
    UBT_COMPILED_PLATFORM=Win64
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    FMT_HEADER_ONLY=1
    _UNICODE
    UNICODE
    WIN32_LEAN_AND_MEAN
)
# UE4SS's public GUI header emits C5038 under MSVC 19.44, so this adapter
# cannot use /WX globally. The standalone core remains compiled with /WX.
target_compile_options(PalResourceFactoryNative PRIVATE /W4 /EHsc /utf-8)
target_link_options(PalResourceFactoryNative PRIVATE
    "/MAP:${CMAKE_BINARY_DIR}/main.map"
    "/MAPINFO:EXPORTS"
)
target_link_libraries(PalResourceFactoryNative PRIVATE UE4SSRuntime)
set_target_properties(PalResourceFactoryNative PROPERTIES OUTPUT_NAME "main")

# Manual, non-consuming experiment. Never links the quarantined facility runtime.
add_library(PalResourceFactoryProbe SHARED EXCLUDE_FROM_ALL src/DirectSettlementProbe.cpp)
target_include_directories(PalResourceFactoryProbe PRIVATE
    $<TARGET_PROPERTY:PalResourceFactoryNative,INCLUDE_DIRECTORIES>)
target_compile_definitions(PalResourceFactoryProbe PRIVATE
    $<TARGET_PROPERTY:PalResourceFactoryNative,COMPILE_DEFINITIONS>)
target_compile_options(PalResourceFactoryProbe PRIVATE /W4 /EHsc /utf-8)
target_link_libraries(PalResourceFactoryProbe PRIVATE UE4SSRuntime)
set_target_properties(PalResourceFactoryProbe PROPERTIES OUTPUT_NAME "main"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/probe"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/probe")

# Deliberately separate artifact: Ctrl+Alt+F9 performs irreversible test settlement.
# The proven Ctrl+Alt+F8 preview above remains non-consuming.
add_library(PalResourceFactorySettlementTest SHARED EXCLUDE_FROM_ALL src/DirectSettlementProbe.cpp)
target_include_directories(PalResourceFactorySettlementTest PRIVATE
    $<TARGET_PROPERTY:PalResourceFactoryNative,INCLUDE_DIRECTORIES>)
target_compile_definitions(PalResourceFactorySettlementTest PRIVATE PRF_MANUAL_SETTLEMENT_TEST=1
    $<TARGET_PROPERTY:PalResourceFactoryNative,COMPILE_DEFINITIONS>)
target_compile_options(PalResourceFactorySettlementTest PRIVATE /W4 /EHsc /utf-8)
target_link_libraries(PalResourceFactorySettlementTest PRIVATE UE4SSRuntime)
set_target_properties(PalResourceFactorySettlementTest PROPERTIES OUTPUT_NAME "main"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/settlement-test"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/settlement-test")

add_library(PalResourceFactoryProcessor SHARED EXCLUDE_FROM_ALL src/EggProcessor.cpp)
target_include_directories(PalResourceFactoryProcessor PRIVATE
    $<TARGET_PROPERTY:PalResourceFactoryNative,INCLUDE_DIRECTORIES>)
target_compile_definitions(PalResourceFactoryProcessor PRIVATE
    $<TARGET_PROPERTY:PalResourceFactoryNative,COMPILE_DEFINITIONS>)
target_compile_options(PalResourceFactoryProcessor PRIVATE /W4 /EHsc /utf-8)
target_link_libraries(PalResourceFactoryProcessor PRIVATE UE4SSRuntime)
set_target_properties(PalResourceFactoryProcessor PROPERTIES OUTPUT_NAME "main"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/processor"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/processor")

# Isolated, manually triggered read-only integration check. Not the breeder runtime.
# Parked farm-generation experiment; not the selected ancient breeder route.
add_library(PalResourceFactoryBreederGeneration OBJECT EXCLUDE_FROM_ALL src/BreederGenerationCompile.cpp)
target_include_directories(PalResourceFactoryBreederGeneration PRIVATE
    $<TARGET_PROPERTY:PalResourceFactoryNative,INCLUDE_DIRECTORIES>)
target_compile_definitions(PalResourceFactoryBreederGeneration PRIVATE
    $<TARGET_PROPERTY:PalResourceFactoryNative,COMPILE_DEFINITIONS>)
target_compile_options(PalResourceFactoryBreederGeneration PRIVATE /W4 /EHsc /utf-8)

add_library(PalResourceFactoryAncientBreederAdapter OBJECT EXCLUDE_FROM_ALL src/AncientBreederAdapterCompile.cpp)
target_include_directories(PalResourceFactoryAncientBreederAdapter PRIVATE
    $<TARGET_PROPERTY:PalResourceFactoryNative,INCLUDE_DIRECTORIES>)
target_compile_definitions(PalResourceFactoryAncientBreederAdapter PRIVATE
    $<TARGET_PROPERTY:PalResourceFactoryNative,COMPILE_DEFINITIONS>)
target_compile_options(PalResourceFactoryAncientBreederAdapter PRIVATE /W4 /EHsc /utf-8)

add_library(PalResourceFactoryBreederProbe SHARED EXCLUDE_FROM_ALL src/BreederProbe.cpp)
add_library(PalResourceFactoryAncientBreeder SHARED EXCLUDE_FROM_ALL src/AncientBreeder.cpp)
target_include_directories(PalResourceFactoryAncientBreeder PRIVATE
    $<TARGET_PROPERTY:PalResourceFactoryNative,INCLUDE_DIRECTORIES>)
target_compile_definitions(PalResourceFactoryAncientBreeder PRIVATE
    $<TARGET_PROPERTY:PalResourceFactoryNative,COMPILE_DEFINITIONS>)
target_compile_options(PalResourceFactoryAncientBreeder PRIVATE /W4 /EHsc /utf-8)
target_link_libraries(PalResourceFactoryAncientBreeder PRIVATE UE4SSRuntime)
set_target_properties(PalResourceFactoryAncientBreeder PROPERTIES OUTPUT_NAME "main"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ancient-breeder"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ancient-breeder")
target_include_directories(PalResourceFactoryBreederProbe PRIVATE
    $<TARGET_PROPERTY:PalResourceFactoryNative,INCLUDE_DIRECTORIES>)
target_compile_definitions(PalResourceFactoryBreederProbe PRIVATE
    $<TARGET_PROPERTY:PalResourceFactoryNative,COMPILE_DEFINITIONS>)
target_compile_options(PalResourceFactoryBreederProbe PRIVATE /W4 /EHsc /utf-8)
target_link_libraries(PalResourceFactoryBreederProbe PRIVATE UE4SSRuntime)
set_target_properties(PalResourceFactoryBreederProbe PROPERTIES OUTPUT_NAME "main"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breeder-probe"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breeder-probe")
