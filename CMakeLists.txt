cmake_minimum_required(VERSION 4.0)
project(CW_Parallel_Computing)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Bin)

set(DATA_PATH "${CMAKE_SOURCE_DIR}/Data")
add_compile_definitions(DATA_DIR_PATH="${DATA_PATH}")

include_directories(${CMAKE_SOURCE_DIR}/ThirdParty)

add_executable(search_server
        Src/Server/main.cpp
        Src/Server/SearchServer.cpp Src/Server/SearchServer.h
        Src/Server/Document.cpp Src/Server/Document.h
        Src/Server/DocumentReader.cpp Src/Server/DocumentReader.h
        Src/Server/InvertedIndex.cpp Src/Server/InvertedIndex.h
        Src/Server/ThreadPool.cpp Src/Server/ThreadPool.h
        Src/Server/FileWatcher.cpp Src/Server/FileWatcher.h
)

if (WIN32)
    target_link_libraries(search_server ws2_32 wsock32)
    target_compile_definitions(search_server PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

add_executable(search_client
        Src/Client/main.cpp
        Src/Client/SearchClient.cpp Src/Client/SearchClient.h
        Src/Client/SearchResultPrinter.cpp
        Src/Client/SearchResultPrinter.h
)

if (WIN32)
    target_link_libraries(search_client ws2_32 wsock32)
    target_compile_definitions(search_client PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")