cmake_minimum_required(VERSION 3.16)

project(Zrun LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt库
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

# 添加源文件
set(ZRUN_SOURCES
    zrun_core.cpp
    zrun_c.cpp
    zrun_cpp.cpp
    ZRunQt.cpp
)

# 添加头文件
set(ZRUN_HEADERS
    zrun_types.h
    zrun_core.h
    zrun.h
    zrun.hpp
    ZRunQt.h
)

# 创建静态库而不是共享库
add_library(Zrun STATIC ${ZRUN_SOURCES} ${ZRUN_HEADERS})

# 设置包含目录
target_include_directories(Zrun PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 链接Qt库
target_link_libraries(Zrun PRIVATE Qt${QT_VERSION_MAJOR}::Core)

# 设置编译定义 - 移除DLL相关定义
target_compile_definitions(Zrun PRIVATE ZRUN_LIBRARY)

# 如果是Windows，添加静态编译定义
if(WIN32)
    target_compile_definitions(Zrun PRIVATE ZRUN_STATIC)
endif()
