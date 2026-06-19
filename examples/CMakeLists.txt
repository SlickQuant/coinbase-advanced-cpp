set(EXAMPLES
    market_data_ws_callbacks
    market_data_user_thread_callbacks
    multi_websockets_ws_callbacks
    multi_websockets_user_thread_callbacks
    multi_websockets_ws_callbacks_reader
)

foreach(tgt ${EXAMPLES})
    add_executable(${tgt} ${tgt}.cpp)
    target_link_libraries(${tgt} PRIVATE coinbase-advanced-cpp)
    target_include_directories(${tgt} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    if (MSVC)
        target_compile_options(${tgt} PRIVATE /MP /FS /bigobj /W4)
        target_compile_definitions(${tgt} PRIVATE _WIN32_WINNT=0x0A00)
    else()
        target_compile_options(${tgt} PRIVATE -Wall -Wextra)
    endif()
endforeach()
