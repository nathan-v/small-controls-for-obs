# Headless test suite for the dock widget.
#
# The widget is compiled straight into the test binary and linked against a
# fake libobs (obs-stub.cpp) instead of the real one, so only the OBS headers
# are needed and no OBS install or running frontend is involved. The harness
# is plain Qt Widgets: obs-deps does not ship QtTest, and this way the suite
# builds wherever the plugin builds. The binary selects the offscreen
# platform itself.

add_library(obs-stub STATIC obs-stub.cpp obs-stub.hpp)
target_include_directories(
  obs-stub
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    $<TARGET_PROPERTY:OBS::libobs,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:OBS::obs-frontend-api,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(obs-stub PUBLIC Qt6::Widgets)
target_compile_definitions(
  obs-stub
  PRIVATE
    SMALL_CONTROLS_LOCALE_FILE="${CMAKE_SOURCE_DIR}/data/locale/en-US.ini"
    SMALL_CONTROLS_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
)

add_executable(
  small-controls-tests
  harness.cpp
  harness.hpp
  test-small-controls.cpp
  "${CMAKE_SOURCE_DIR}/src/small-controls.cpp"
  "${CMAKE_SOURCE_DIR}/src/small-controls.hpp"
)
set_target_properties(small-controls-tests PROPERTIES AUTOMOC ON)
target_include_directories(small-controls-tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(small-controls-tests PRIVATE obs-stub Qt6::Widgets)

# Same Qt header noise the plugin target silences.
foreach(_target IN ITEMS obs-stub small-controls-tests)
  target_compile_options(
    ${_target}
    PRIVATE $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
endforeach()

if(OS_MACOS)
  # The plugin borrows Qt from OBS.app at runtime; the test binary has to find
  # the downloaded Qt frameworks itself, and hardened runtime would refuse to
  # load them under an ad-hoc signature. Qt6_DIR is <prefix>/lib/cmake/Qt6.
  get_filename_component(_qt_lib_dir "${Qt6_DIR}/../.." ABSOLUTE)
  set_target_properties(
    small-controls-tests
    PROPERTIES XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "${_qt_lib_dir}" XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME NO
  )
endif()

if(OS_WINDOWS)
  # No rpath on Windows: the test run needs the downloaded Qt's bin/ on PATH.
  # Qt6_DIR is <prefix>/lib/cmake/Qt6.
  get_filename_component(_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
  set(_test_env "PATH=path_list_prepend:${_qt_prefix}/bin")
  if(NOT EXISTS "${_qt_prefix}/plugins/platforms/qoffscreen.dll")
    # Qt bundle without the offscreen plugin: run on the real platform instead.
    list(APPEND _test_env "QT_QPA_PLATFORM=set:windows")
  endif()
endif()

add_test(NAME small-controls COMMAND small-controls-tests)
if(_test_env)
  set_tests_properties(small-controls PROPERTIES ENVIRONMENT_MODIFICATION "${_test_env}")
endif()
