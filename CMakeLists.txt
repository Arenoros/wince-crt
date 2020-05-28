cmake_minimum_required(2.8)

project(wince-crt C)

set(SOURCES 
    ChildData.c
    crt.c
    env.c
    locale.c
    pipe.c
    redir.c
    stat.c
    time.c
    ts_string.c
)
set(HEADERS 
    args.h
    ChildData.h
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
    sys/stat.h
    sys/timeb.h
)

add_library(wince-crt STATIC ${SOURCES})
set_target_properties(wince-crt
        PROPERTIES PUBLIC_HEADER "${HEADERS}")
        
# install(FILES ${HEADERS}
#     DESTINATION include COMPONENT dev_headers)

install(TARGETS wince-crt
    ARCHIVE DESTINATION lib COMPONENT libraries
    PUBLIC_HEADER DESTINATION include COMPONENT dev)