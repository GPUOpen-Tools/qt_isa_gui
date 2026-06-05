cmake_minimum_required (VERSION 3.24)

# Add header files.
file (GLOB CPP_INC
    "isa_dictionary.h"
    "shader_source_syntax_highlighter.h"
)

# Add source files.
file (GLOB CPP_SRC
    "isa_dictionary.cpp"
    "shader_source_syntax_highlighter.cpp"
)

add_library(qt_isa_utility STATIC ${CPP_SRC} ${CPP_INC})

set_target_properties(qt_isa_utility PROPERTIES AUTOMOC ON)

target_include_directories(qt_isa_utility PUBLIC ${PROJECT_SOURCE_DIR}/source)

target_link_libraries(qt_isa_utility PUBLIC
                                     Qt6::Gui
                                     QtUtils)

devtools_target_options(qt_isa_utility)
