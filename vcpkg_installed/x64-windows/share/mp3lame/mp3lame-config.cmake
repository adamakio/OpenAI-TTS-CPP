
get_filename_component(_mp3lame_root "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(_mp3lame_root "${_mp3lame_root}" PATH)
get_filename_component(_mp3lame_root "${_mp3lame_root}" PATH)

set(_mp3lame_rel_lib "${_mp3lame_root}/lib/libmp3lame.lib")
set(_mp3lame_dbg_lib "${_mp3lame_root}/debug/lib/libmp3lame.lib")

if (EXISTS "${_mp3lame_rel_lib}" OR EXISTS "${_mp3lame_dbg_lib}")

    add_library(mp3lame::mp3lame UNKNOWN IMPORTED)
    set_target_properties(mp3lame::mp3lame 
        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_mp3lame_root}/include")

    if (EXISTS "${_mp3lame_rel_lib}")
        set_target_properties(mp3lame::mp3lame 
            PROPERTIES IMPORTED_LOCATION_RELEASE "${_mp3lame_rel_lib}")
        set_property(TARGET mp3lame::mp3lame APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
    endif()
    if (EXISTS "${_mp3lame_dbg_lib}")
        set_target_properties(mp3lame::mp3lame 
            PROPERTIES IMPORTED_LOCATION_DEBUG "${_mp3lame_dbg_lib}")
        set_property(TARGET mp3lame::mp3lame APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
    endif()

    set(_mp3lame_mpghip_rel_lib "${_mp3lame_root}/lib/libmpghip-static.lib")
    set(_mp3lame_mpghip_dbg_lib "${_mp3lame_root}/debug/lib/libmpghip-static.lib")

    if (EXISTS "${_mp3lame_mpghip_rel_lib}" OR EXISTS "${_mp3lame_mpghip_dbg_lib}")

        add_library(mp3lame::mpghip UNKNOWN IMPORTED)

        if (EXISTS "${_mp3lame_rel_lib}")
            set_target_properties(mp3lame::mpghip 
                PROPERTIES IMPORTED_LOCATION_RELEASE "${_mp3lame_mpghip_rel_lib}")
            set_property(TARGET mp3lame::mpghip APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
        endif()
        if (EXISTS "${_mp3lame_dbg_lib}")
            set_target_properties(mp3lame::mpghip 
                PROPERTIES IMPORTED_LOCATION_DEBUG "${_mp3lame_mpghip_dbg_lib}")
            set_property(TARGET mp3lame::mpghip APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
        endif()

        set_target_properties(mp3lame::mp3lame PROPERTIES INTERFACE_LINK_LIBRARIES mp3lame::mpghip)
        
    endif()

    unset(_mp3lame_mpghip_rel_lib)
    unset(_mp3lame_mpghip_dbg_lib)

else()

    set(mp3lame_FOUND FALSE)

endif()

unset(_mp3lame_rel_lib)
unset(_mp3lame_dbg_lib)

unset(_mp3lame_root)
