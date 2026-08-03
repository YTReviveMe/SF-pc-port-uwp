include_guard(GLOBAL)

set(_SF_XBOX_DEVMODE_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

set(SF_UWP_DEPENDENCY_ROOT "" CACHE PATH
    "Root of the WorleyDL UWP dependency bundle (contains x64/bin and x64/lib)")
set(SF_UWP_OPENAL_INCLUDE_ROOT "" CACHE PATH
    "Directory containing OpenAL headers al.h, alc.h, alext.h and efx.h")
set(SF_UWP_FFMPEG_INCLUDE_ROOT "" CACHE PATH
    "FFmpeg include root containing libavcodec/avcodec.h")
set(SF_UWP_FFMPEG_LIBRARY_ROOT "" CACHE PATH
    "FFmpeg library root containing avcodec.lib, avformat.lib, avutil.lib, swresample.lib and swscale.lib")
set(SF_UWP_FFMPEG_RUNTIME_ROOT "" CACHE PATH
    "Directory containing the matching UWP FFmpeg runtime DLLs")

function(sf_configure_xbox_devmode_dependencies)
    if(NOT SF_UWP_DEPENDENCY_ROOT)
        message(FATAL_ERROR
            "SF_XBOX_UWP requires SF_UWP_DEPENDENCY_ROOT. Point it at the "
            "WorleyDL uwp-dep checkout that contains x64/bin/SDL2.dll.")
    endif()
    if(NOT SF_UWP_OPENAL_INCLUDE_ROOT)
        message(FATAL_ERROR
            "SF_XBOX_UWP requires SF_UWP_OPENAL_INCLUDE_ROOT. Point it at a "
            "current OpenAL Soft include directory containing al.h and alext.h.")
    endif()
    if(NOT SF_UWP_FFMPEG_INCLUDE_ROOT OR NOT SF_UWP_FFMPEG_LIBRARY_ROOT OR
       NOT SF_UWP_FFMPEG_RUNTIME_ROOT)
        message(FATAL_ERROR
            "SF_XBOX_UWP requires SF_UWP_FFMPEG_INCLUDE_ROOT and "
            "SF_UWP_FFMPEG_LIBRARY_ROOT plus SF_UWP_FFMPEG_RUNTIME_ROOT so "
            "STR movies stay enabled.")
    endif()

    set(SF_UWP_X64_ROOT "${SF_UWP_DEPENDENCY_ROOT}/x64" PARENT_SCOPE)
    set(_sf_uwp_bin "${SF_UWP_DEPENDENCY_ROOT}/x64/bin")
    set(_sf_uwp_lib "${SF_UWP_DEPENDENCY_ROOT}/x64/lib")
    foreach(_required_file IN ITEMS
        "${SF_UWP_DEPENDENCY_ROOT}/x64/include/SDL2/SDL.h"
        "${_sf_uwp_lib}/SDL2.lib"
        "${_sf_uwp_bin}/SDL2.dll"
        "${_sf_uwp_bin}/OpenAL32.dll"
        "${_sf_uwp_bin}/opengl32.dll"
        "${SF_UWP_OPENAL_INCLUDE_ROOT}/al.h"
        "${SF_UWP_OPENAL_INCLUDE_ROOT}/alext.h"
        "${SF_UWP_FFMPEG_INCLUDE_ROOT}/libavcodec/avcodec.h"
        "${SF_UWP_FFMPEG_LIBRARY_ROOT}/avcodec.lib"
        "${SF_UWP_FFMPEG_LIBRARY_ROOT}/avformat.lib"
        "${SF_UWP_FFMPEG_LIBRARY_ROOT}/avutil.lib"
        "${SF_UWP_FFMPEG_LIBRARY_ROOT}/swresample.lib"
        "${SF_UWP_FFMPEG_LIBRARY_ROOT}/swscale.lib")
        if(NOT EXISTS "${_required_file}")
            message(FATAL_ERROR "Required Xbox UWP dependency is missing: ${_required_file}")
        endif()
    endforeach()

    add_library(sf_uwp_sdl SHARED IMPORTED GLOBAL)
    set_target_properties(sf_uwp_sdl PROPERTIES
        IMPORTED_LOCATION "${_sf_uwp_bin}/SDL2.dll"
        IMPORTED_IMPLIB "${_sf_uwp_lib}/SDL2.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${SF_UWP_DEPENDENCY_ROOT}/x64/include/SDL2")
    add_library(SDL2::SDL2 ALIAS sf_uwp_sdl)

    add_library(sf_uwp_opengl INTERFACE)
    target_link_libraries(sf_uwp_opengl INTERFACE opengl32.lib WindowsApp.lib)
    add_library(OpenGL::GL ALIAS sf_uwp_opengl)

    set(_sf_openal_import_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/uwp-openal")
    set(_sf_openal_import_library "${_sf_openal_import_dir}/OpenAL32.lib")
    file(MAKE_DIRECTORY "${_sf_openal_import_dir}")
    execute_process(
        COMMAND "${CMAKE_AR}" /nologo /machine:x64
            "/def:${_SF_XBOX_DEVMODE_CMAKE_DIR}/UwpOpenAL32.def"
            "/out:${_sf_openal_import_library}"
        RESULT_VARIABLE _sf_openal_import_result
        OUTPUT_VARIABLE _sf_openal_import_output
        ERROR_VARIABLE _sf_openal_import_error)
    if(NOT _sf_openal_import_result EQUAL 0 OR NOT EXISTS "${_sf_openal_import_library}")
        message(FATAL_ERROR
            "Cannot generate the UWP OpenAL32 import library with ${CMAKE_AR}: "
            "${_sf_openal_import_output}${_sf_openal_import_error}")
    endif()
    add_library(sf_uwp_openal SHARED IMPORTED GLOBAL)
    set_target_properties(sf_uwp_openal PROPERTIES
        IMPORTED_LOCATION "${_sf_uwp_bin}/OpenAL32.dll"
        IMPORTED_IMPLIB "${_sf_openal_import_library}"
        INTERFACE_INCLUDE_DIRECTORIES
            "${CMAKE_CURRENT_SOURCE_DIR}/external/openal_uwp_compat;${SF_UWP_OPENAL_INCLUDE_ROOT}")
    add_library(OpenAL::OpenAL ALIAS sf_uwp_openal)

    add_library(sf_uwp_ffmpeg INTERFACE)
    target_include_directories(sf_uwp_ffmpeg INTERFACE "${SF_UWP_FFMPEG_INCLUDE_ROOT}")
    target_link_libraries(sf_uwp_ffmpeg INTERFACE
        "${SF_UWP_FFMPEG_LIBRARY_ROOT}/avformat.lib"
        "${SF_UWP_FFMPEG_LIBRARY_ROOT}/avcodec.lib"
        "${SF_UWP_FFMPEG_LIBRARY_ROOT}/swscale.lib"
        "${SF_UWP_FFMPEG_LIBRARY_ROOT}/swresample.lib"
        "${SF_UWP_FFMPEG_LIBRARY_ROOT}/avutil.lib"
        bcrypt.lib ws2_32.lib uuid.lib ole32.lib oleaut32.lib shlwapi.lib strmiids.lib)

    # FFmpeg ports can bring along additional UWP DLLs (for example libxml2 or
    # OpenSSL).  Validate the five required media components explicitly, then
    # deploy the complete matching runtime directory so AppX has their
    # transitive dynamic dependencies as well.
    set(_sf_uwp_ffmpeg_runtime_files)
    foreach(_ffmpeg_component IN ITEMS avcodec avformat avutil swresample swscale)
        file(GLOB _ffmpeg_component_runtime
            "${SF_UWP_FFMPEG_RUNTIME_ROOT}/${_ffmpeg_component}*.dll")
        if(NOT _ffmpeg_component_runtime)
            message(FATAL_ERROR
                "The matching ${_ffmpeg_component} runtime DLL was not found under "
                "SF_UWP_FFMPEG_RUNTIME_ROOT=${SF_UWP_FFMPEG_RUNTIME_ROOT}")
        endif()
    endforeach()
    file(GLOB _sf_uwp_ffmpeg_runtime_files
        "${SF_UWP_FFMPEG_RUNTIME_ROOT}/*.dll")
    list(REMOVE_DUPLICATES _sf_uwp_ffmpeg_runtime_files)

    set(SF_UWP_RUNTIME_FILES
        "${_sf_uwp_bin}/SDL2.dll"
        "${_sf_uwp_bin}/libuwp.dll"
        "${_sf_uwp_bin}/opengl32.dll"
        "${_sf_uwp_bin}/libgallium_wgl.dll"
        "${_sf_uwp_bin}/dxil.dll"
        "${_sf_uwp_bin}/z-1.dll"
        "${_sf_uwp_bin}/OpenAL32.dll"
        ${_sf_uwp_ffmpeg_runtime_files}
        PARENT_SCOPE)
endfunction()
