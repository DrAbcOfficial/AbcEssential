cmake_minimum_required(VERSION 3.24)

project(AbcEssential LANGUAGES CXX)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/AbcCommon.cmake")

set(ABC_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/AbcCore")
add_subdirectory("${ABC_CORE_DIR}" AbcCore-build)

set(METAHOOK_INCLUDE_DIR "${ABC_CORE_DIR}/thirdparty/MetaHook/include")

add_library(AbcEssential MODULE
    src/plugins.cpp
    src/hooks.cpp
    src/services/Config.cpp
    src/services/ExtraPrecache.cpp
    src/modules/Module.cpp
    src/modules/Find/Find.cpp
    src/modules/Models/Models.cpp
    src/modules/AutoFunc/AutoFunc.cpp
    src/modules/AutoFunc/Signals.cpp
    src/modules/ViewModelLag/ViewModelLag.cpp
    src/modules/Camera/Camera.cpp
    src/modules/Efx/Efx.cpp
    # Generic CreateInterface export (EXPOSE_SINGLE_INTERFACE implementation)
    "${METAHOOK_INCLUDE_DIR}/HLSDK/common/interface.cpp"
)
target_compile_definitions(AbcEssential PRIVATE WIN32 _WINDOWS _USRDLL _CRT_SECURE_NO_WARNINGS)
target_link_libraries(AbcEssential PRIVATE Abc::Core)

if(ABC_GAME_DIR)
    install(TARGETS AbcEssential
        RUNTIME DESTINATION "${ABC_GAME_DIR}/svencoop/metahook/plugins"
        LIBRARY DESTINATION "${ABC_GAME_DIR}/svencoop/metahook/plugins")
    add_custom_target(deploy
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --config "$<CONFIG>"
        COMMENT "Deploying AbcEssential.dll to ${ABC_GAME_DIR}/svencoop/metahook/plugins")
endif()
