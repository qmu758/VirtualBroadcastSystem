cmake_minimum_required(VERSION 3.24)
project(VirtualBroadcastSystem VERSION 0.1.0 LANGUAGES CXX)
option(VB_ENABLE_STRESS_TESTS "Build compatibility and stress fixtures" OFF)
option(VB_ENABLE_MSVC_ANALYZE "Enable MSVC /analyze" OFF)
if(NOT WIN32)
  message(FATAL_ERROR "This project is Windows-only")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  add_compile_options(/utf-8)
endif()

add_library(vb_common STATIC
  src/common/log.cpp
  src/common/pipe_protocol.cpp
  src/common/runtime_config.cpp
  src/broadcaster/channel/channel.cpp
  src/broadcaster/common/mpeg_crc32.cpp
  src/broadcaster/common/ts_section.cpp)
target_include_directories(vb_common PUBLIC include)
target_compile_definitions(vb_common PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

add_library(vb_broadcaster_core STATIC
  src/broadcaster/input/obs_input.cpp
  src/broadcaster/data/data_broadcast.cpp
  src/broadcaster/diagnostics/diagnostics.cpp
  src/broadcaster/mux/pat_generator.cpp
  src/broadcaster/mux/pmt_generator.cpp
  src/broadcaster/mux/ts_muxer.cpp
  src/broadcaster/si/arib_string_encoder.cpp
  src/broadcaster/logo/logo.cpp
  src/broadcaster/si/cdt_generator.cpp
  src/broadcaster/si/descriptor_writer.cpp
  src/broadcaster/si/epg.cpp
  src/broadcaster/si/eit_generators.cpp
  src/broadcaster/si/si_generators.cpp
  src/broadcaster/si/time_encoding.cpp
  src/broadcaster/transport/named_pipe_server.cpp
  src/broadcaster/transport/ts_output_router.cpp)
target_include_directories(vb_broadcaster_core PUBLIC include)
target_link_libraries(vb_broadcaster_core PUBLIC vb_common ws2_32 windowscodecs ole32)

add_executable(VirtualBroadcaster
  src/broadcaster/app/main.cpp
  src/broadcaster/app/virtual_broadcaster.cpp)
target_link_libraries(VirtualBroadcaster PRIVATE vb_broadcaster_core)

enable_testing()
add_executable(vb_tests tests/unit/milestone1_tests.cpp)
target_link_libraries(vb_tests PRIVATE vb_broadcaster_core)
add_test(NAME milestone1_psi_ts COMMAND vb_tests)

add_executable(vb_milestone3_tests tests/unit/milestone3_tests.cpp)
target_link_libraries(vb_milestone3_tests PRIVATE vb_broadcaster_core)
add_test(NAME milestone3_si_tables COMMAND vb_milestone3_tests)

add_executable(vb_milestone4_tests tests/unit/milestone4_tests.cpp)
target_link_libraries(vb_milestone4_tests PRIVATE vb_broadcaster_core)
add_test(NAME milestone4_eit COMMAND vb_milestone4_tests)

add_executable(vb_milestone5_tests tests/unit/milestone5_tests.cpp)
target_link_libraries(vb_milestone5_tests PRIVATE vb_broadcaster_core)
target_compile_definitions(vb_milestone5_tests PRIVATE VB_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME milestone5_cdt_logo COMMAND vb_milestone5_tests)
add_executable(vb_milestone6_tests tests/integration/milestone6_tests.cpp)
target_link_libraries(vb_milestone6_tests PRIVATE vb_broadcaster_core vb_bondriver_core)
target_compile_definitions(vb_milestone6_tests PRIVATE VB_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME milestone6_channels COMMAND vb_milestone6_tests)
add_executable(vb_milestone7_tests tests/integration/milestone7_tests.cpp)
target_link_libraries(vb_milestone7_tests PRIVATE vb_broadcaster_core)
target_compile_definitions(vb_milestone7_tests PRIVATE VB_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME milestone7_data_carousel COMMAND vb_milestone7_tests)
if(VB_ENABLE_STRESS_TESTS)
  add_executable(vb_milestone8_tests tests/compatibility/milestone8_tests.cpp)
  target_link_libraries(vb_milestone8_tests PRIVATE vb_broadcaster_core vb_bondriver_core)
  target_compile_definitions(vb_milestone8_tests PRIVATE VB_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  add_test(NAME milestone8_compatibility_stress COMMAND vb_milestone8_tests)
endif()
add_executable(vb_generate_sample_ts tools/generate_sample_ts.cpp)
target_link_libraries(vb_generate_sample_ts PRIVATE vb_broadcaster_core)

add_library(vb_bondriver_core STATIC
  src/bondriver/pipe_client.cpp
  src/bondriver/ts_ring_buffer.cpp)
target_include_directories(vb_bondriver_core PUBLIC include)
target_link_libraries(vb_bondriver_core PUBLIC vb_common)

add_library(BonDriver_Virtual SHARED
  src/bondriver/bon_driver_virtual.cpp
  src/bondriver/exports.def)
target_link_libraries(BonDriver_Virtual PRIVATE vb_bondriver_core)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set_target_properties(BonDriver_Virtual PROPERTIES PREFIX "" OUTPUT_NAME "BonDriver_Virtual_x64")
else()
  set_target_properties(BonDriver_Virtual PROPERTIES PREFIX "" OUTPUT_NAME "BonDriver_Virtual_x86")
endif()

add_executable(vb_milestone2_tests tests/integration/milestone2_tests.cpp)
target_link_libraries(vb_milestone2_tests PRIVATE vb_broadcaster_core vb_bondriver_core)
add_test(NAME milestone2_pipe_buffer COMMAND vb_milestone2_tests)

add_executable(vb_dll_abi_tests tests/integration/dll_abi_tests.cpp)
target_include_directories(vb_dll_abi_tests PRIVATE include)
target_link_libraries(vb_dll_abi_tests PRIVATE vb_broadcaster_core)
add_dependencies(vb_dll_abi_tests BonDriver_Virtual)
add_test(NAME milestone2_dll_export_abi COMMAND vb_dll_abi_tests $<TARGET_FILE:BonDriver_Virtual>)
set_tests_properties(milestone1_psi_ts milestone3_si_tables milestone4_eit milestone5_cdt_logo PROPERTIES LABELS "unit")
set_tests_properties(milestone2_pipe_buffer milestone2_dll_export_abi milestone6_channels milestone7_data_carousel PROPERTIES LABELS "integration")
if(VB_ENABLE_STRESS_TESTS)
  set_tests_properties(milestone8_compatibility_stress PROPERTIES LABELS "compatibility;stress")
endif()

if(MSVC)
  set(vb_test_targets vb_tests vb_milestone2_tests vb_milestone3_tests vb_milestone4_tests vb_milestone5_tests vb_milestone6_tests vb_milestone7_tests vb_dll_abi_tests)
  if(VB_ENABLE_STRESS_TESTS)
    list(APPEND vb_test_targets vb_milestone8_tests)
  endif()
  foreach(test_target ${vb_test_targets})
    target_compile_options(${test_target} PRIVATE /UNDEBUG)
  endforeach()
  if(VB_ENABLE_MSVC_ANALYZE)
    target_compile_options(vb_common PRIVATE /W4 /analyze)
    target_compile_options(vb_broadcaster_core PRIVATE /W4 /analyze)
    target_compile_options(vb_bondriver_core PRIVATE /W4 /analyze)
  endif()
endif()
