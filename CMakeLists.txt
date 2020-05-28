cmake_minimum_required(VERSION 2.8.12)

project(wince-crt C)

set(SOURCES
    crt.c
    env.cpp
    locale.c
    pipe.cpp
    stat.c
    time.c
)
set(HEADERS 
    args.h
    conio.h
    crt.h
    errno.h
    fcntl.h
    internal.h
    pipe.h
    redir.h
    signal.h
    stat.h
    stat_wce.h
    stdio.h
    time_WC9000.h
    ts_string.h
    io.h
    sys/stat.h
    sys/timeb.h
)

add_library(wince-crt STATIC ${SOURCES})
set_target_properties(wince-crt
        PROPERTIES PUBLIC_HEADER "${HEADERS}")

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
        
# install(FILES ${HEADERS}
#     DESTINATION include COMPONENT dev_headers)

install(DIRECTORY sys DESTINATION include/sys)
install(TARGETS wince-crt
    ARCHIVE DESTINATION lib COMPONENT libraries
    PUBLIC_HEADER DESTINATION include COMPONENT dev)
