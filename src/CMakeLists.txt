add_library(psax_core
    util/binary_reader.cpp
    pac/arc_types.cpp
    pac/pac_file.cpp
    psa/misc_section.cpp
    psa/arg.cpp
    psa/event.cpp
    psa/command_table.cpp
    psa/requirements_table.cpp
    psa/event_decoder.cpp
    psa/character_root.cpp
    psa/subaction_table.cpp
    psa/subroutine_scan.cpp
    psa/subaction_flags.cpp
    psa/sfx_audit.cpp
)
target_include_directories(psax_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(psax main.cpp)
target_link_libraries(psax PRIVATE psax_core)
