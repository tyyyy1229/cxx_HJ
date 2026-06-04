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


)



target_link_libraries(bellhop_2D_interface PUBLIC bellhopcxxstatic Threads::Threads)
target_include_directories(bellhop_2D_interface  PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_include_directories(bellhop_2D_interface  PUBLIC "${CMAKE_SOURCE_DIR}/glm")
target_include_directories(bellhop_2D_interface  PUBLIC "${CMAKE_SOURCE_DIR}")

