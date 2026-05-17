cmake_minimum_required(VERSION 3.10)
project(DA2026_PRJ2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(myProg
        src/main.cpp
        src/menu.cpp
        src/parser.cpp
        src/web.cpp
        src/allocator.cpp
        src/output.cpp
        include/types.h
        include/parser.h
        include/menu.h
        include/web.h
        include/allocator.h
        include/output.h
        include/data_structures/Graph.h
)
