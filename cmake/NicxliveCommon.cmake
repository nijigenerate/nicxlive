# Canonical SDK common source provider, independently usable without renderer dependencies.
# Both the SDK and its real application consumers link this one target.
if(NOT TARGET nicxlive_common)
    get_filename_component(_nicxlive_common_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    find_package(Threads REQUIRED)
    add_library(nicxlive_common STATIC
        "${_nicxlive_common_root}/core/compat/static_error.cpp"
        "${_nicxlive_common_root}/core/compat/static_error.hpp"
        "${_nicxlive_common_root}/core/compat/system_error.hpp"
        "${_nicxlive_common_root}/core/compat/switch_error.hpp"
        "${_nicxlive_common_root}/core/compat/thread_exception.cpp"
        "${_nicxlive_common_root}/core/compat/thread_exception.hpp"
        "${_nicxlive_common_root}/core/compat/source_mutex.hpp"
        "${_nicxlive_common_root}/core/compat/source_mutex.cpp"
        "${_nicxlive_common_root}/core/compat/source_condition.hpp"
        "${_nicxlive_common_root}/core/compat/source_condition.cpp"
        "${_nicxlive_common_root}/core/compat/phobos_stdio.cpp"
        "${_nicxlive_common_root}/core/compat/phobos_stdio.hpp"
        "${_nicxlive_common_root}/core/compat/phobos_conv_exception.hpp"
        "${_nicxlive_common_root}/core/fmt/io.hpp"
        "${_nicxlive_common_root}/core/fmt/io.cpp"
        "${_nicxlive_common_root}/core/compat/default_random.cpp"
        "${_nicxlive_common_root}/core/compat/default_random.hpp"
        "${_nicxlive_common_root}/core/compat/retained_string.cpp"
        "${_nicxlive_common_root}/core/compat/object_metadata.cpp"
        "${_nicxlive_common_root}/core/compat/native_access_fault.cpp"
        "${_nicxlive_common_root}/core/common/unicode_lowercase.cpp"
        "${_nicxlive_common_root}/core/common/unicode_lowercase.hpp")
    target_sources(nicxlive_common PUBLIC
        "${_nicxlive_common_root}/core/compat/source_allocator.hpp"
        "${_nicxlive_common_root}/core/common/string_key_map.hpp"
        "${_nicxlive_common_root}/core/serde_json.hpp"
        "${_nicxlive_common_root}/core/serialization_fields.hpp")
    add_library(nicxlive::common ALIAS nicxlive_common)
    target_include_directories(nicxlive_common PUBLIC "${_nicxlive_common_root}/core")
    target_compile_features(nicxlive_common PUBLIC cxx_std_20)
    target_link_libraries(nicxlive_common PUBLIC Threads::Threads)
    set_target_properties(nicxlive_common PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
