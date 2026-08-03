cmake_minimum_required(VERSION 3.24)

# CMake selects the UWP toolchain during project() rather than after it. The
# cache value is set by the Xbox preset (or -DSF_XBOX_UWP=ON).
if(DEFINED SF_XBOX_UWP AND SF_XBOX_UWP)
    set(CMAKE_SYSTEM_NAME WindowsStore CACHE STRING "Target the UWP SDK" FORCE)
    set(CMAKE_SYSTEM_VERSION 10.0.19041.0 CACHE STRING "UWP SDK version" FORCE)
endif()

if(POLICY CMP0174)
    cmake_policy(SET CMP0174 NEW)
endif()

project(SyphonFilterPC VERSION 0.1.0 LANGUAGES C CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(SF_BUILD_TESTS "Build unit tests" ON)
option(SF_ENABLE_PSYCROSS "Build the optional PsyCross platform backend" OFF)
option(SF_XBOX_UWP "Build the Xbox Dev Mode UWP package" OFF)
option(SF_XBOX_LTCG "Use link-time code generation for Xbox Release builds" ON)
option(SF_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(SF_PARALLEL_COMPILE "Compile independent sources in parallel" ON)
set(SF_PSYCROSS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/PsyCross" CACHE PATH
    "Path to a PsyCross checkout")
set(SF_SUPPORTED_ROM_CUE "" CACHE FILEPATH
    "Optional Syphon Filter USA v1.1 CUE used by supported-ROM CTest gates")
set(_sf_rom_probes_default OFF)
if(SF_SUPPORTED_ROM_CUE)
    set(_sf_rom_probes_default ON)
endif()
option(SF_BUILD_ROM_PROBES "Build supported-ROM diagnostic probes"
    ${_sf_rom_probes_default})
unset(_sf_rom_probes_default)

# The retail compatibility VM executes millions of small interpreter
# operations per guest frame. Let MSVC inline across our static libraries in
# the Xbox Release package; this is especially valuable for the VM's memory
# and instruction-dispatch fast paths. It is intentionally UWP-only so the
# normal desktop iteration loop remains fast.
if(SF_XBOX_UWP AND MSVC AND SF_XBOX_LTCG)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(SfWarnings)
if(SF_XBOX_UWP)
    include(SfXboxDevMode)
    sf_configure_xbox_devmode_dependencies()
    add_compile_definitions(
        SF_XBOX_UWP=1
        __WINRT__=1
        WINAPI_FAMILY=WINAPI_FAMILY_APP
        NOMINMAX)
endif()

add_library(sf_core
    src/core/file_io.cpp
    src/core/sha256.cpp
)
add_library(sf::core ALIAS sf_core)
target_include_directories(sf_core PUBLIC include)
target_compile_features(sf_core PUBLIC cxx_std_20)
sf_set_project_warnings(sf_core)

add_library(sf_assets
    src/assets/emd_scene.cpp
    src/assets/fog_archive.cpp
    src/assets/gmd_model.cpp
    src/assets/hmd_animation.cpp
    src/assets/hmd_model.cpp
    src/assets/hog_archive.cpp
    src/assets/level_layout.cpp
    src/assets/mission_briefing.cpp
    src/assets/mission_objects.cpp
    src/assets/tim_image.cpp
    src/assets/weapon_descriptions.cpp
)
add_library(sf::assets ALIAS sf_assets)
target_include_directories(sf_assets PUBLIC include)
target_compile_features(sf_assets PUBLIC cxx_std_20)
target_link_libraries(sf_assets PUBLIC sf::core)
sf_set_project_warnings(sf_assets)

add_library(sf_disc
    src/disc/cue_sheet.cpp
    src/disc/iso9660.cpp
    src/disc/raw_sector_source.cpp
)
add_library(sf::disc ALIAS sf_disc)
target_include_directories(sf_disc PUBLIC include)
target_compile_features(sf_disc PUBLIC cxx_std_20)
target_link_libraries(sf_disc PUBLIC sf::core)
sf_set_project_warnings(sf_disc)

add_library(sf_psx
    src/psx/cdrom.cpp
    src/psx/dma.cpp
    src/psx/event_scheduler.cpp
    src/psx/executable.cpp
    src/psx/function_map.cpp
    src/psx/gte_runtime.cpp
    src/psx/interrupt_controller.cpp
    src/psx/machine.cpp
    src/psx/r3000_runtime.cpp
    src/psx/spu.cpp
    src/psx/timers.cpp
    src/psx/vab_decoder.cpp
    src/psx/xa_decoder.cpp
)
add_library(sf::psx ALIAS sf_psx)
target_include_directories(sf_psx PUBLIC include)
target_compile_features(sf_psx PUBLIC cxx_std_20)
target_link_libraries(sf_psx PUBLIC sf::core)
sf_set_project_warnings(sf_psx)

add_library(sf_platform_input
    src/platform/player_input.cpp
)
add_library(sf::platform_input ALIAS sf_platform_input)
target_include_directories(sf_platform_input PUBLIC include)
target_compile_features(sf_platform_input PUBLIC cxx_std_20)
sf_set_project_warnings(sf_platform_input)

add_library(sf_xbox_content
    src/platform/xbox_game_image.cpp
)
add_library(sf::xbox_content ALIAS sf_xbox_content)
target_include_directories(sf_xbox_content PUBLIC include)
target_compile_features(sf_xbox_content PUBLIC cxx_std_20)
target_link_libraries(sf_xbox_content PUBLIC sf::core sf::disc)
sf_set_project_warnings(sf_xbox_content)

add_library(sf_game
    src/game/actor_animation.cpp
    src/game/campaign.cpp
    src/game/chase_camera.cpp
    src/game/combat.cpp
    src/game/dynamic_lighting.cpp
    src/game/effects.cpp
    src/game/gameplay.cpp
    src/game/game_disc.cpp
    src/game/hud.cpp
    src/game/legacy_first_mission_runtime.cpp
    src/game/legacy_gameplay_vm.cpp
    src/game/legacy_mission_image.cpp
    src/game/legacy_presentation_bridge.cpp
    src/game/legacy_virtual_cd.cpp
    src/game/localization.cpp
    src/game/mission.cpp
    src/game/mission_start.cpp
    src/game/mission_scripts.cpp
    src/game/npc_ai.cpp
    src/game/pause_menu.cpp
    src/game/pause_menu_data.cpp
    src/game/player_controller.cpp
    src/game/retail_pause_map.cpp
    src/game/retail_cheats.cpp
    src/game/state_stack.cpp
    src/game/supported_games.cpp
    src/game/system.cpp
    src/game/title.cpp
)
add_library(sf::game ALIAS sf_game)
target_include_directories(sf_game PUBLIC include)
target_compile_features(sf_game PUBLIC cxx_std_20)
target_link_libraries(sf_game PUBLIC sf::assets sf::core sf::disc sf::psx)
if(MSVC)
    target_compile_options(sf_game PRIVATE /utf-8)
endif()
sf_set_project_warnings(sf_game)

if(NOT SF_XBOX_UWP)
    add_executable(sf_tool
        apps/sf_tool/main.cpp
        apps/sf_tool/ui_export.cpp
    )
    target_link_libraries(sf_tool PRIVATE sf::game)
    sf_set_project_warnings(sf_tool)
endif()

include(SfRomProbes)

if(SF_ENABLE_PSYCROSS)
    if(NOT EXISTS "${SF_PSYCROSS_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR "PsyCross was not found at SF_PSYCROSS_ROOT=${SF_PSYCROSS_ROOT}")
    endif()

    if(SF_XBOX_UWP)
        set(FFMPEG_INCLUDE_DIRS "${SF_UWP_FFMPEG_INCLUDE_ROOT}")
        set(FFMPEG_LIBRARIES sf_uwp_ffmpeg)
    else()
        find_package(SDL2 CONFIG REQUIRED)
        find_package(OpenAL CONFIG REQUIRED)
        find_package(OpenGL REQUIRED)
        find_package(FFMPEG REQUIRED)
    endif()

    # PsyCross links a plain `OpenAL` item. Make that name a real target so the
    # final executable receives vcpkg's correctly named OpenAL32 import library.
    if(NOT TARGET OpenAL)
        add_library(OpenAL INTERFACE)
        target_link_libraries(OpenAL INTERFACE OpenAL::OpenAL)
    endif()

    add_subdirectory("${SF_PSYCROSS_ROOT}" "${CMAKE_BINARY_DIR}/_deps/psycross")
    if(MSVC AND SF_PARALLEL_COMPILE)
        target_compile_options(psycross_static PRIVATE /MP)
    endif()

    add_library(sf_media
        src/media/str_decoder.cpp
    )
    add_library(sf::media ALIAS sf_media)
    target_include_directories(sf_media PUBLIC include)
    target_include_directories(sf_media SYSTEM PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_directories(sf_media PRIVATE ${FFMPEG_LIBRARY_DIRS})
    target_compile_features(sf_media PUBLIC cxx_std_20)
    target_link_libraries(sf_media PRIVATE sf::core ${FFMPEG_LIBRARIES})
    sf_set_project_warnings(sf_media)

    add_library(sf_psycross_backend
        src/platform/psycross_audio_output.cpp
        src/platform/psycross_font_texture.cpp
        src/platform/psycross_host.cpp
        src/platform/psycross_mission_start.cpp
        src/platform/psycross_movie_player.cpp
        src/platform/psycross_retail_briefing.cpp
        src/platform/psycross_runtime_guards.cpp
        src/platform/psycross_scene_viewer.cpp
        src/platform/psycross_scene_effects.inc
        src/platform/psycross_scene_hud.inc
        src/platform/psycross_scene_models.inc
        src/platform/psycross_scene_pause.inc
        src/platform/psycross_scene_presentation.inc
        src/platform/psycross_scene_render_core.inc
        src/platform/psycross_scene_runtime.inc
        src/platform/psycross_scene_save_renderer.inc
        src/platform/psycross_scene_textures.inc
        src/platform/psycross_scene_world.inc
        src/platform/psycross_vram.cpp
        src/platform/psycross_window_mode.cpp
    )
    add_library(sf::psycross_backend ALIAS sf_psycross_backend)
    target_include_directories(sf_psycross_backend PUBLIC include)
    target_compile_features(sf_psycross_backend PUBLIC cxx_std_20)
    target_link_libraries(sf_psycross_backend
        PUBLIC sf::game sf::platform_input
        PRIVATE sf::media psycross_static OpenAL::OpenAL OpenGL::GL SDL2::SDL2)
    sf_set_project_warnings(sf_psycross_backend)

    if(SF_XBOX_UWP)
        add_library(sf_xbox_launcher STATIC
            apps/syphon_filter/launcher_xbox.cpp
        )
        target_include_directories(sf_xbox_launcher PRIVATE include)
        target_compile_features(sf_xbox_launcher PRIVATE cxx_std_17)
        target_link_libraries(sf_xbox_launcher PRIVATE SDL2::SDL2)
        if(MSVC)
            # The 19041 C++/WinRT headers use MSVC's C++17 coroutine surface.
            target_compile_options(sf_xbox_launcher PRIVATE /await /permissive)
        endif()

        add_executable(syphon_filter WIN32
            apps/syphon_filter/main.cpp
            apps/syphon_filter/xbox_entry.cpp
        )
    else()
        add_executable(syphon_filter
            apps/syphon_filter/launcher.cpp
            apps/syphon_filter/main.cpp
        )
    endif()
    if(WIN32 AND NOT SF_XBOX_UWP)
        set(SF_LAUNCHER_ICON_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/syphon_filter.ico")
        set(SF_LAUNCHER_ICON_SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher/gabe_launcher_icon.png")
        if(NOT EXISTS "${SF_LAUNCHER_ICON_SOURCE}")
            message(FATAL_ERROR "Cannot find the Gabriel Logan launcher icon source")
        endif()
        execute_process(
            COMMAND "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe"
                -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_gabe_launcher_icon.ps1"
                -OutputPath "${SF_LAUNCHER_ICON_OUTPUT}"
                -SourcePath "${SF_LAUNCHER_ICON_SOURCE}"
            RESULT_VARIABLE SF_LAUNCHER_ICON_RESULT)
        if(NOT SF_LAUNCHER_ICON_RESULT EQUAL 0 OR
           NOT EXISTS "${SF_LAUNCHER_ICON_OUTPUT}")
            message(FATAL_ERROR "Cannot generate the Syphon Filter launcher icon")
        endif()
        set(SF_LAUNCHER_ICON_PATH "syphon_filter.ico")
        configure_file(
            apps/syphon_filter/syphon_filter.rc.in
            "${CMAKE_CURRENT_BINARY_DIR}/syphon_filter.rc"
            @ONLY)
        target_sources(syphon_filter PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/syphon_filter.rc")
    endif()
    target_link_libraries(syphon_filter PRIVATE sf::psycross_backend)
    if(SF_XBOX_UWP)
        target_link_libraries(syphon_filter PRIVATE
            sf_xbox_launcher sf::xbox_content SDL2::SDL2)
    endif()
    add_custom_command(TARGET syphon_filter POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/locales"
            "$<TARGET_FILE_DIR:syphon_filter>/locales"
        VERBATIM)
    if(WIN32 AND NOT SF_XBOX_UWP)
        target_link_libraries(syphon_filter PRIVATE user32 comdlg32 gdiplus)
        set(SF_DOSSIER_SCREEN_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/dossiers/screens")
        set(SF_DOSSIER_SCREENS
            dossier_01.png
            dossier_02.png
            dossier_03.png
            dossier_04.png)
        foreach(SF_DOSSIER_SCREEN IN LISTS SF_DOSSIER_SCREENS)
            if(NOT EXISTS "${SF_DOSSIER_SCREEN_DIR}/${SF_DOSSIER_SCREEN}")
                message(FATAL_ERROR
                    "Missing dossier screen: ${SF_DOSSIER_SCREEN}")
            endif()
        endforeach()
        add_custom_command(TARGET syphon_filter POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_FILE_DIR:syphon_filter>/assets/dossiers/screens"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${SF_DOSSIER_SCREEN_DIR}/dossier_01.png"
                "${SF_DOSSIER_SCREEN_DIR}/dossier_02.png"
                "${SF_DOSSIER_SCREEN_DIR}/dossier_03.png"
                "${SF_DOSSIER_SCREEN_DIR}/dossier_04.png"
                "$<TARGET_FILE_DIR:syphon_filter>/assets/dossiers/screens"
            VERBATIM)
    endif()
    if(SF_XBOX_UWP)
        set(SF_UWP_PACKAGE_MANIFEST
            "${CMAKE_CURRENT_SOURCE_DIR}/apps/syphon_filter/Package.appxmanifest")
        set(SF_UWP_PACKAGE_ICON_SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher/syphon_filter_uwp_cover.jpg")
        set(SF_UWP_PACKAGE_ASSET_DIRECTORY
            "${CMAKE_CURRENT_BINARY_DIR}/uwp-assets")
        set(SF_UWP_PACKAGE_ASSETS
            "${SF_UWP_PACKAGE_ASSET_DIRECTORY}/StoreLogo.png"
            "${SF_UWP_PACKAGE_ASSET_DIRECTORY}/Square44x44Logo.png"
            "${SF_UWP_PACKAGE_ASSET_DIRECTORY}/Square150x150Logo.png"
            "${SF_UWP_PACKAGE_ASSET_DIRECTORY}/SplashScreen.png")
        execute_process(
            COMMAND "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe"
                -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_uwp_visual_assets.ps1"
                -OutputDirectory "${SF_UWP_PACKAGE_ASSET_DIRECTORY}"
                -SourcePath "${SF_UWP_PACKAGE_ICON_SOURCE}"
            RESULT_VARIABLE SF_UWP_PACKAGE_ASSET_RESULT)
        if(NOT SF_UWP_PACKAGE_ASSET_RESULT EQUAL 0)
            message(FATAL_ERROR "Unable to generate UWP package visual assets")
        endif()
        set_source_files_properties(
            "${SF_UWP_PACKAGE_MANIFEST}" PROPERTIES
            VS_DEPLOYMENT_CONTENT TRUE
            VS_DEPLOYMENT_LOCATION ".")
        set_source_files_properties(
            ${SF_UWP_PACKAGE_ASSETS} PROPERTIES
            VS_COPY_TO_OUT_DIR Always
            VS_DEPLOYMENT_CONTENT TRUE
            VS_DEPLOYMENT_LOCATION "assets/launcher")
        target_sources(syphon_filter PRIVATE
            "${SF_UWP_PACKAGE_MANIFEST}"
            ${SF_UWP_PACKAGE_ASSETS})
        file(GLOB_RECURSE SF_UWP_LOCALE_FILES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/*")
        foreach(SF_UWP_LOCALE_FILE IN LISTS SF_UWP_LOCALE_FILES)
            if(IS_DIRECTORY "${SF_UWP_LOCALE_FILE}")
                continue()
            endif()
            file(RELATIVE_PATH SF_UWP_LOCALE_RELATIVE
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/locales"
                "${SF_UWP_LOCALE_FILE}")
            get_filename_component(SF_UWP_LOCALE_DIRECTORY
                "${SF_UWP_LOCALE_RELATIVE}" DIRECTORY)
            if(SF_UWP_LOCALE_DIRECTORY)
                set(SF_UWP_LOCALE_DEPLOYMENT_LOCATION
                    "locales/${SF_UWP_LOCALE_DIRECTORY}")
            else()
                set(SF_UWP_LOCALE_DEPLOYMENT_LOCATION "locales")
            endif()
            set_source_files_properties("${SF_UWP_LOCALE_FILE}" PROPERTIES
                VS_COPY_TO_OUT_DIR Always
                VS_DEPLOYMENT_CONTENT TRUE
                VS_DEPLOYMENT_LOCATION "${SF_UWP_LOCALE_DEPLOYMENT_LOCATION}")
            target_sources(syphon_filter PRIVATE "${SF_UWP_LOCALE_FILE}")
        endforeach()
        foreach(SF_UWP_RUNTIME_FILE IN LISTS SF_UWP_RUNTIME_FILES)
            if(NOT EXISTS "${SF_UWP_RUNTIME_FILE}")
                message(FATAL_ERROR "Missing UWP runtime file: ${SF_UWP_RUNTIME_FILE}")
            endif()
            get_filename_component(SF_UWP_RUNTIME_NAME "${SF_UWP_RUNTIME_FILE}" NAME)
            set_source_files_properties("${SF_UWP_RUNTIME_FILE}" PROPERTIES
                VS_COPY_TO_OUT_DIR Always
                VS_DEPLOYMENT_CONTENT TRUE
                VS_DEPLOYMENT_LOCATION ".")
            target_sources(syphon_filter PRIVATE "${SF_UWP_RUNTIME_FILE}")
            add_custom_command(TARGET syphon_filter POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${SF_UWP_RUNTIME_FILE}"
                    "$<TARGET_FILE_DIR:syphon_filter>/${SF_UWP_RUNTIME_NAME}"
                VERBATIM)
        endforeach()
    endif()
    sf_set_project_warnings(syphon_filter)

    if(NOT SF_XBOX_UWP)
        add_executable(sf_movie_probe apps/sf_movie_probe/main.cpp)
        target_link_libraries(sf_movie_probe PRIVATE sf::game sf::media)
        sf_set_project_warnings(sf_movie_probe)
    endif()
endif()

include(SfTests)
