cmake_minimum_required(VERSION 3.24)

project(QtIsaGui)

# Define version information
set(QTISAGUI_MAJOR_VERSION 1)
set(QTISAGUI_MINOR_VERSION 0)

if (NOT QTISAGUI_PATCH_NUMBER)
    set(QTISAGUI_PATCH_NUMBER 0)
endif ()

if (NOT QTISAGUI_BUILD_NUMBER)
    set(QTISAGUI_BUILD_NUMBER 0)
endif ()

string(TIMESTAMP DATE "\"%m/%d/%Y\"")
string(TIMESTAMP YEAR "\"%Y\"")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Check if top-level project before attempting to query Qt dependencies
# This allows for consuming projects to override the Qt version used such as using
# Qt6 instead of Qt5
if (PROJECT_IS_TOP_LEVEL)

    include(dev_tools)
    include(devtools_qt_helper)

    add_subdirectory(test)

endif ()

# Before fetching the isa_decoder, set this variable so we only pull in 
# the isa_decoder, and skip the command line interface, examples, and tests.
set (EXCLUDE_ISA_CLI_EXAMPLES_TESTS ON)

# Fetch external dependencies; qt_common, isa_decoder and its xml files.
include(qt_isa_gui_external)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Buildinfo.properties.in" "${CMAKE_CURRENT_SOURCE_DIR}/Buildinfo.properties")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/source/version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/source/version.h")

# Isa Gui Widgets.
add_subdirectory(source/qt_isa_gui/widgets)

# Isa Gui Utility.
add_subdirectory(source/qt_isa_gui/utility)

# Packaging
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_GROUPING IGNORE)

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    set(CPACK_ARCHIVE_ISAGUI_EXAMPLE_FILE_NAME "QtIsaGui_Example_Debug-${QTISAGUI_MAJOR_VERSION}.${QTISAGUI_MINOR_VERSION}.${QTISAGUI_PATCH_NUMBER}.${QTISAGUI_BUILD_NUMBER}")
else ()
    set(CPACK_ARCHIVE_ISAGUI_EXAMPLE_FILE_NAME "QtIsaGui_Example_${QTISAGUI_MAJOR_VERSION}.${QTISAGUI_MINOR_VERSION}.${QTISAGUI_PATCH_NUMBER}.${QTISAGUI_BUILD_NUMBER}")
endif ()

include(CPack)

cpack_add_component(Example
        DISPLAY_NAME "IsaGui Example"
        DESCRIPTION "QtIsaGui Example Application")

IF(WIN32)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Example)
ENDIF(WIN32)

# Copy the isa spec files for the example application.
if (PROJECT_IS_TOP_LEVEL)

    # Determine where the build process will be placing the binary files
    # This is evaluated at project build time - not at CMake generation time
    set(BUILD_ROOT $<$<CONFIG:Debug>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}>$<$<CONFIG:Release>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>)
    
    set(ISA_SPEC_OUTPUT_DIR ${BUILD_ROOT})
    
    add_custom_target(ISA_SPEC ALL)
    
    # Copy the isa xml files into a common location.
    file(GLOB ISA_SPEC_FILES ${CMAKE_SOURCE_DIR}/external/isa_spec_manager_xml/amdgpu_isa_*.xml)

    add_custom_command(TARGET ISA_SPEC POST_BUILD
    
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ISA_SPEC_OUTPUT_DIR}/utils/isa_spec
    
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ISA_SPEC_FILES} ${ISA_SPEC_OUTPUT_DIR}/utils/isa_spec/.       
    )

endif ()