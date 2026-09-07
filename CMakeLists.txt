cmake_minimum_required(VERSION 3.25)
project(PalResourceFactoryCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(PalResourceFactoryCore STATIC
    src/core/FacilityState.cpp
)
target_include_directories(PalResourceFactoryCore PUBLIC src/core)
if(MSVC)
    target_compile_options(PalResourceFactoryCore PRIVATE /W4 /WX /EHsc /utf-8)
else()
    target_compile_options(PalResourceFactoryCore PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(facility_state_test tests/facility_state_test.cpp)
target_link_libraries(facility_state_test PRIVATE PalResourceFactoryCore)
if(MSVC)
    target_compile_options(facility_state_test PRIVATE /W4 /WX /EHsc /utf-8)
else()
    target_compile_options(facility_state_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_test(NAME facility_state_test COMMAND facility_state_test)

add_executable(settlement_types_test tests/settlement_types_test.cpp)
target_compile_options(settlement_types_test PRIVATE $<TARGET_PROPERTY:facility_state_test,COMPILE_OPTIONS>)
add_test(NAME settlement_types_test COMMAND settlement_types_test)

add_executable(processor_state_test tests/processor_state_test.cpp)
target_compile_options(processor_state_test PRIVATE $<TARGET_PROPERTY:facility_state_test,COMPILE_OPTIONS>)
add_test(NAME processor_state_test COMMAND processor_state_test)

add_executable(async_disk_test tests/async_disk_test.cpp)
target_compile_options(async_disk_test PRIVATE $<TARGET_PROPERTY:facility_state_test,COMPILE_OPTIONS>)
add_test(NAME async_disk_test COMMAND async_disk_test)

add_executable(breeder_state_test tests/breeder_state_test.cpp)
target_compile_options(breeder_state_test PRIVATE $<TARGET_PROPERTY:facility_state_test,COMPILE_OPTIONS>)
add_test(NAME breeder_state_test COMMAND breeder_state_test)
