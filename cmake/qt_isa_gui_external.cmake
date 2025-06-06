#######################################################################################################################
### Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
### @author AMD Developer Tools Team
#######################################################################################################################

include(FetchContent)

set (GITHUB_REPO_PREFIX "https://github.com/GPUOpen-Tools")
set (QT_COMMON_BRANCH "v4.3.0")
set (ISA_SPEC_MANAGER_BRANCH "v1.1.0")

if (NOT TARGET QtCustomWidgets AND NOT TARGET QtUtils)
    if (NOT QTCOMMON_DIR)
        # QtCommon
        FetchContent_Declare(
                qt_common
                GIT_REPOSITORY "${GITHUB_REPO_PREFIX}/qt_common.git"
                GIT_TAG "${QT_COMMON_BRANCH}"
                SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/qt_common"
        )
        FetchContent_MakeAvailable(qt_common)
    else ()
        add_subdirectory(${QTCOMMON_DIR} qt_common)
    endif ()
endif ()

if (NOT TARGET isa_decoder)
    if (NOT ISA_SPEC_MANAGER_DIR)
        # isa_spec_manager
        FetchContent_Declare(
                isa_spec_manager
                GIT_REPOSITORY "${GITHUB_REPO_PREFIX}/isa_spec_manager.git"
                GIT_TAG "${ISA_SPEC_MANAGER_BRANCH}"
                SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/isa_spec_manager"
        )
        FetchContent_MakeAvailable(isa_spec_manager)
    else ()
        add_subdirectory(${ISA_SPEC_MANAGER_DIR} isa_spec_manager)
    endif ()
    
    set(ISA_DECODER_XML_URL "https://gpuopen.com/download/machine-readable-isa/latest")
    set(ISA_DECODER_XML_DIR "${PROJECT_SOURCE_DIR}/external/isa_spec_xml")

    FetchContent_Declare(
       isa_spec_xml
       URL ${ISA_DECODER_XML_URL}
       SOURCE_DIR ${ISA_DECODER_XML_DIR}
       DOWNLOAD_EXTRACT_TIMESTAMP true
    )

    message(STATUS "Downloading ${ISA_DECODER_XML_URL} into ${ISA_DECODER_XML_DIR}")

    FetchContent_MakeAvailable(isa_spec_xml)

    if (EXISTS "${ISA_DECODER_XML_DIR}")
        message(STATUS "ISA decoder XML directory created successfully at ${ISA_DECODER_XML_DIR}")
    else ()
        message(FATAL_ERROR "Directory ${ISA_DECODER_XML_DIR} was not created.")
    endif ()
endif ()
