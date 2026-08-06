add_library(pskLibC STATIC
    main.c
)

if(WIN32)
    target_sources(pskLibC PRIVATE
        system/systemWindows.c
        udp/udpWindows.c
    )
elseif(UNIX)
    target_sources(pskLibC PRIVATE
        system/systemPosix.c
        udp/udpPosix.c
    )
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

target_include_directories(pskLibC
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(pskLibC
    PRIVATE
        m
)
