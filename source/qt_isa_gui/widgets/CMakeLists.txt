cmake_minimum_required (VERSION 3.24)

# Build Qt .ui MOC files for the library.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Add header files.
file (GLOB CPP_INC
    "isa_branch_label_navigation_widget.h"
    "isa_item_delegate.h"
    "isa_item_model.h"
    "isa_proxy_model.h"
    "isa_tooltip.h"
    "isa_tree_view.h"
    "isa_widget.h"
    "isa_vertical_scroll_bar.h"
)

# Add .ui files.
file (GLOB UI_SRC
    "isa_widget.ui"
)

# Add source files.
file (GLOB CPP_SRC
    "isa_branch_label_navigation_widget.cpp"
    "isa_item_delegate.cpp"
    "isa_item_model.cpp"
    "isa_proxy_model.cpp"
    "isa_tooltip.cpp"
    "isa_tree_view.cpp"
    "isa_widget.cpp"
    "isa_vertical_scroll_bar.cpp"
)

add_library(qt_isa_widgets STATIC ${CPP_SRC} ${CPP_INC} ${UI_SRC})

if (PROJECT_IS_TOP_LEVEL)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    set_target_properties(QtCustomWidgets QtUtils isa_decoder isa_decoder_dynamic PROPERTIES FOLDER Dependencies)
endif ()

target_include_directories(qt_isa_widgets PUBLIC ${PROJECT_SOURCE_DIR}/source)

target_link_libraries(qt_isa_widgets PUBLIC
                                     Qt::Widgets
                                     QtCustomWidgets
                                     isa_decoder
                                     PRIVATE
                                     QtUtils
                                     qt_isa_utility)

devtools_target_options(qt_isa_widgets)
