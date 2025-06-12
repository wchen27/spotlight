cmake_minimum_required(VERSION 3.10)
project(Spotlight CXX)

set(CMAKE_CXX_STANDARD 11)

# Dependencies
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
pkg_search_module(GLEW REQUIRED glew)

include_directories(
    ${GLFW_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
    third_party/imgui
    third_party/imgui/backends
    serial
)

add_definitions(${GLFW_CFLAGS_OTHER})
add_definitions(${GLEW_CFLAGS_OTHER})

# ImGui source files
set(IMGUI_SOURCES
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_demo.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
    serial/serial.cpp
)

# Main executable
add_executable(spotlight spotlight.cpp ${IMGUI_SOURCES})

target_link_libraries(spotlight
    ${GLFW_LIBRARIES}
    ${GLEW_LIBRARIES}
    ${OPENGL_LIBRARIES}
    dl
)
