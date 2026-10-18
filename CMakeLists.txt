cmake_minimum_required(VERSION 4.2)

project(AbcEssential LANGUAGES CXX)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/AbcCommon.cmake")

# AbcCore is owned by the aggregate ABCEnchance repository and consumed as a
# shared library through ABC_CORE_DIR. Standalone builds use a sibling
# checkout by default.
set(ABC_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../AbcCore" CACHE PATH
    "Path to the shared AbcCore source tree")
if(NOT EXISTS "${ABC_CORE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "AbcCore not found at '${ABC_CORE_DIR}'. Clone DrAbcOfficial/AbcCore as a sibling of AbcEssential or pass -DABC_CORE_DIR=<path>.")
endif()
add_subdirectory("${ABC_CORE_DIR}" AbcCore-build)

set(METAHOOK_INCLUDE_DIR "${ABC_CORE_DIR}/thirdparty/MetaHook/include")

add_library(AbcEssential MODULE
    src/plugins.cpp
    src/CoreGlobals.cpp
    src/hooks.cpp
    src/services/Config.cpp
    src/services/ExtraPrecache.cpp
    src/modules/Module.cpp
    src/modules/find/Find.cpp
    src/modules/models/Models.cpp
    src/modules/autofunc/AutoFunc.cpp
    src/modules/autofunc/Signals.cpp
    src/modules/viewmodellag/ViewModelLag.cpp
    src/modules/camera/Camera.cpp
    src/modules/fx/Efx.cpp
    src/modules/fx/Blood.cpp
    src/modules/fx/Shell.cpp
    src/modules/fx/Gauss.cpp
    src/modules/fx/Egon.cpp
    # Generic CreateInterface export (EXPOSE_SINGLE_INTERFACE implementation)
    "${METAHOOK_INCLUDE_DIR}/HLSDK/common/interface.cpp"
)
target_compile_definitions(AbcEssential PRIVATE WIN32 _WINDOWS _USRDLL _CRT_SECURE_NO_WARNINGS)
target_compile_options(AbcEssential PRIVATE /W4 /FIabc/ForceImport.h)
target_compile_definitions(AbcEssential PRIVATE ABC_CORE_STATIC)
target_link_libraries(AbcEssential PRIVATE Abc::Core)

include("${ABC_CORE_DIR}/cmake/AbcGameData.cmake")
abc_add_gamedata(AbcEssential "${CMAKE_CURRENT_SOURCE_DIR}/scripts/manifests/abcessential.json")

if(ABC_GAME_DIR)
    install(TARGETS AbcEssential
        RUNTIME DESTINATION "${ABC_GAME_DIR}/svencoop/metahook/plugins"
        LIBRARY DESTINATION "${ABC_GAME_DIR}/svencoop/metahook/plugins")
    add_custom_target(deploy
        DEPENDS AbcEssential
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --config "$<CONFIG>"
        COMMENT "Deploying AbcEssential.dll to ${ABC_GAME_DIR}/svencoop/metahook/plugins")
endif()

# --- Packaging ---------------------------------------------------------------
# `cmake --build <build-dir> --config Release --target dist` produces
# AbcEssential-<version>.zip, laid out to unpack into the Sven Co-op root:
#   AbcEssential-<version>/svencoop/metahook/plugins/AbcEssential.dll
#   AbcEssential-<version>/svencoop/metahook/gamedata/abcessential/** (generated symbols)
#   AbcEssential-<version>/README.md
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/services/Version.h" ABC_ESSENTIAL_VERSION_LINE
    REGEX "kVersion")
string(REGEX MATCH "\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" ABC_ESSENTIAL_VERSION "${ABC_ESSENTIAL_VERSION_LINE}")
set(ABC_ESSENTIAL_VERSION "${CMAKE_MATCH_1}")
if(NOT ABC_ESSENTIAL_VERSION)
    message(WARNING "Could not parse the AbcEssential version from src/services/Version.h; packaging as 0.0.0")
    set(ABC_ESSENTIAL_VERSION "0.0.0")
endif()
set(ABC_ESSENTIAL_STAGE_DIR "${CMAKE_BINARY_DIR}/dist/AbcEssential-${ABC_ESSENTIAL_VERSION}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/dist")
add_custom_target(dist
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${ABC_ESSENTIAL_STAGE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${ABC_ESSENTIAL_STAGE_DIR}/svencoop/metahook/plugins"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:AbcEssential>"
        "${ABC_ESSENTIAL_STAGE_DIR}/svencoop/metahook/plugins/"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_BINARY_DIR}/assets/svencoop/metahook/gamedata/abcessential"
        "${ABC_ESSENTIAL_STAGE_DIR}/svencoop/metahook/gamedata/abcessential"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/README.md" "${ABC_ESSENTIAL_STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E tar cfv "AbcEssential-${ABC_ESSENTIAL_VERSION}.zip" --format=zip
        "AbcEssential-${ABC_ESSENTIAL_VERSION}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
    DEPENDS AbcEssential AbcEssentialGameData
    COMMENT "Packaging AbcEssential-${ABC_ESSENTIAL_VERSION}.zip")
