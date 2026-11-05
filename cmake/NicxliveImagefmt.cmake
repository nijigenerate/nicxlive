# Canonical independent imagefmt 2.1.2 source dependency; shared by SDK and application.
if(NOT TARGET nicxlive_imagefmt)
    get_filename_component(_nicxlive_imagefmt_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    include("${CMAKE_CURRENT_LIST_DIR}/NicxliveCommon.cmake")
    if(NOT TARGET ZLIB::ZLIB)
        find_package(ZLIB REQUIRED)
    endif()
    add_library(nicxlive_imagefmt STATIC
        "${_nicxlive_imagefmt_root}/core/imagefmt/nt_string.hpp"
        "${_nicxlive_imagefmt_root}/core/imagefmt/writer.cpp"
        "${_nicxlive_imagefmt_root}/core/imagefmt/bmp.cpp"
        "${_nicxlive_imagefmt_root}/core/imagefmt/tga.cpp"
        "${_nicxlive_imagefmt_root}/core/imagefmt/png.cpp"
        "${_nicxlive_imagefmt_root}/core/imagefmt/reader.cpp"
        "${_nicxlive_imagefmt_root}/core/imagefmt/bmp_reader.cpp"
        "${_nicxlive_imagefmt_root}/core/imagefmt/tga_reader.cpp"
        "${_nicxlive_imagefmt_root}/core/imagefmt/png_reader.cpp"
        "${_nicxlive_imagefmt_root}/core/imagefmt/jpeg_reader.cpp")
    add_library(nicxlive::imagefmt ALIAS nicxlive_imagefmt)
    target_include_directories(nicxlive_imagefmt PUBLIC "${_nicxlive_imagefmt_root}/core")
    target_compile_features(nicxlive_imagefmt PUBLIC cxx_std_20)
    target_link_libraries(nicxlive_imagefmt PUBLIC nicxlive::common PRIVATE ZLIB::ZLIB)
    set_target_properties(nicxlive_imagefmt PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(MSVC)
        target_compile_options(nicxlive_imagefmt PRIVATE /fp:strict)
    else()
        target_compile_options(nicxlive_imagefmt PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
