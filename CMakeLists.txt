cmake_minimum_required(VERSION 3.10)
project(mtop)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Для отладки - раскомментировать при необходимости
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fsanitize=address")

# Добавляем исходники
add_executable(mtop
    main.cpp
    src/Monitor.cpp
    src/MonitorServer.cpp
)

# Включаем директории
target_include_directories(mtop PRIVATE src .)

# Линкуем pthread
target_link_libraries(mtop pthread)
