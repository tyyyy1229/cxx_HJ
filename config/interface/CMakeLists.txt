project(bellhop_2D_interface)
set(CMAKE_CXX_STANDARD 17)

find_package(Threads)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)



include_directories(${CMAKE_CURRENT_SOURCE_DIR})



add_library(${PROJECT_NAME} STATIC
	${srcs}
	bellhopParam.cpp
	botopt.cpp
	topopt.cpp
	runtype.cpp
	nc_process2.cpp
	core_acoustic_internal.cpp
	core_acoustic_single.cpp
	core_acoustic_area.cpp


)



target_link_libraries(bellhop_2D_interface PUBLIC bellhopcxxstatic Threads::Threads netcdf_c++4 netcdf)
target_include_directories(bellhop_2D_interface  PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_include_directories(bellhop_2D_interface  PUBLIC "${CMAKE_SOURCE_DIR}/glm")
target_include_directories(bellhop_2D_interface  PUBLIC "${CMAKE_SOURCE_DIR}")

# ============================================================
# 动态库 - 单点模式 (core_acoustic_single)
# ============================================================
add_library(bellhop_core_single SHARED
	${srcs}
	bellhopParam.cpp
	botopt.cpp
	topopt.cpp
	runtype.cpp
	nc_process2.cpp
	core_acoustic_internal.cpp
	core_acoustic_single.cpp
)

target_link_libraries(bellhop_core_single PUBLIC
	bellhopcxxstatic
	Threads::Threads
	netcdf_c++4
	netcdf
)
target_include_directories(bellhop_core_single PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_include_directories(bellhop_core_single PUBLIC "${CMAKE_SOURCE_DIR}/glm")
target_include_directories(bellhop_core_single PUBLIC "${CMAKE_SOURCE_DIR}")

# ============================================================
# 动态库 - 区域模式 (core_acoustic_area)
# ============================================================
add_library(bellhop_core_area SHARED
	${srcs}
	bellhopParam.cpp
	botopt.cpp
	topopt.cpp
	runtype.cpp
	nc_process2.cpp
	core_acoustic_internal.cpp
	core_acoustic_area.cpp
)

target_link_libraries(bellhop_core_area PUBLIC
	bellhopcxxstatic
	Threads::Threads
	netcdf_c++4
	netcdf
)
target_include_directories(bellhop_core_area PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_include_directories(bellhop_core_area PUBLIC "${CMAKE_SOURCE_DIR}/glm")
target_include_directories(bellhop_core_area PUBLIC "${CMAKE_SOURCE_DIR}")
