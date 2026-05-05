function(revaplayer_set_project_warnings target_name)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    elseif(MSVC)
        target_compile_options(${target_name} PRIVATE
            /W4
            /permissive-
        )
    endif()
endfunction()

