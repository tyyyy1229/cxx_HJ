cmake_minimum_required(VERSION 3.9)
project(bellhopcxx LANGUAGES CXX)

include(../SetupCommon.cmake)

bhc_add_libs_exes(cxx cpp "" "" "")

# ============================================================
# 授权机器码查看工具
#
# 该工具用于客户现场生成本机机器码。
# 最终会生成：
#
#   Linux:   bhc_license_tool
#   Windows: bhc_license_tool.exe
#
# 工具源码位置：
#
#   tools/bhc_license_tool.cpp
#
# 它依赖 bellhopcxxstatic，因为 get_machine_code()
# 的实现位于 src/license.cpp，并且会被编译进核心静态库。
# ============================================================

add_executable(bhc_license_tool
    ${CMAKE_SOURCE_DIR}/tools/bhc_license_tool.cpp
)

target_link_libraries(bhc_license_tool
    PRIVATE
        bellhopcxxstatic
        Threads::Threads
)

target_include_directories(bhc_license_tool
    PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/src"
)
