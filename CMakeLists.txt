cmake_minimum_required(VERSION 2.4)
project(mapdb)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/")

option(PRODUCTION "Option description")


include_directories(/usr/local/include)
include_directories(/usr/include/libxml2)

link_directories(/usr/local/lib)

FIND_PACKAGE(PkgConfig)
PKG_CHECK_MODULES(HIREDIS REQUIRED hiredis)
PKG_CHECK_MODULES(JANSSON REQUIRED jansson)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g")

set(SOURCE_FILES
        src/main.c
        src/map.c
        src/branch.c
        src/vbranch.c
        src/channel.c
        src/helpers.c
        src/io.c
        src/view.c
        src/search.c
        src/lib/rb.c
        src/lib/linklist.c
        src/lib/queue.c
        src/lib/refcnt.c
        src/lib/rqueue.c
        src/app.c )
add_executable(treedb ${SOURCE_FILES})

target_link_libraries(treedb xml2 jansson hiredis sqlite3 event pthread event_pthreads m)

if(PRODUCTION EQUAL 1)
    add_definitions(-DPRODUCTION=1)
endif()