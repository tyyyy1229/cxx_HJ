project(BellhopTest)
set(CMAKE_CXX_STANDARD 17)

find_package(Threads)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})



add_executable(${PROJECT_NAME}
    main.cpp
    demo.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/config/interface)




target_link_libraries(${PROJECT_NAME} PUBLIC -static-libgcc -static-libstdc++ bellhopcxxstatic bellhop_2D_interface)
target_include_directories(${PROJECT_NAME}  PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME}  PUBLIC "${CMAKE_SOURCE_DIR}/glm")
