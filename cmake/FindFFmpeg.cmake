#[[
Locates FFmpeg libraries via pkg-config and exposes them as imported targets:

  FFmpeg::avformat
  FFmpeg::avcodec
  FFmpeg::avfilter
  FFmpeg::avutil
  FFmpeg::swscale
  FFmpeg::swresample
  FFmpeg::FFmpeg   (all of the above combined)

Sets FFmpeg_FOUND when all required components are present.
#]]

find_package(PkgConfig REQUIRED)

set(_ffmpeg_components avformat avcodec avfilter avutil swscale swresample)
set(_ffmpeg_all_found TRUE)

foreach(_comp ${_ffmpeg_components})
    string(TOUPPER ${_comp} _comp_upper)
    pkg_check_modules(PC_${_comp_upper} QUIET lib${_comp})

    if(PC_${_comp_upper}_FOUND)
        if(NOT TARGET FFmpeg::${_comp})
            add_library(FFmpeg::${_comp} INTERFACE IMPORTED)
            target_include_directories(FFmpeg::${_comp} INTERFACE ${PC_${_comp_upper}_INCLUDE_DIRS})
            target_link_libraries(FFmpeg::${_comp} INTERFACE ${PC_${_comp_upper}_LINK_LIBRARIES})
            target_compile_options(FFmpeg::${_comp} INTERFACE ${PC_${_comp_upper}_CFLAGS_OTHER})
        endif()
    else()
        set(_ffmpeg_all_found FALSE)
        message(WARNING "FFmpeg component lib${_comp} not found via pkg-config")
    endif()
endforeach()

if(_ffmpeg_all_found AND NOT TARGET FFmpeg::FFmpeg)
    add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
    target_link_libraries(FFmpeg::FFmpeg INTERFACE
        FFmpeg::avformat
        FFmpeg::avcodec
        FFmpeg::avfilter
        FFmpeg::avutil
        FFmpeg::swscale
        FFmpeg::swresample
    )
endif()

set(FFmpeg_FOUND ${_ffmpeg_all_found})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg DEFAULT_MSG FFmpeg_FOUND)
