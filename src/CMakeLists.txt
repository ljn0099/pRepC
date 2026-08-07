add_library(pRepC STATIC
    main.c
)

if(WIN32)
    target_sources(pRepC PRIVATE
        system/systemWindows.c
        udp/udpWindows.c
    )
elseif(UNIX)
    target_sources(pRepC PRIVATE
        system/systemPosix.c
        udp/udpPosix.c
    )
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

target_include_directories(pRepC
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pRepC
    PRIVATE
        m
        binio
)
