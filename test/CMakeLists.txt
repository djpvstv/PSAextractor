include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)

add_executable(psax_tests
    test_main.cpp
    test_binary_reader.cpp
    test_pac_file.cpp
    test_misc_section.cpp
    test_event_decoder.cpp
)
target_link_libraries(psax_tests PRIVATE psax_core doctest::doctest)
target_compile_definitions(psax_tests PRIVATE
    PSAX_TEST_PAC_DIR="${CMAKE_SOURCE_DIR}/test/pacs"
)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(psax_tests)
