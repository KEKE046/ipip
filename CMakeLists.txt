cmake_minimum_required(VERSION 3.16)

project(ipip VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(OpenGL REQUIRED)
# find_package(glfw3 REQUIRED)

set(CMAKE_CXX_FLAGS "-O3")

set(IMGUI_BACKEND glfw opengl3)

add_subdirectory(glfw EXCLUDE_FROM_ALL)
include_directories(glfw/include)

aux_source_directory(imgui IMGUI_SRC)
foreach(BAK IN ITEMS ${IMGUI_BACKEND})
    list(APPEND IMGUI_SRC imgui/backends/imgui_impl_${BAK}.cpp)
endforeach(BAK IN ${IMGUI_BACKEND})
add_library(imgui ${IMGUI_SRC})
target_include_directories(imgui PUBLIC imgui)
target_link_libraries(imgui glfw ${OPENGL_LIBRARIES} ${CMAKE_DL_LIBS})

aux_source_directory(implot IMPLOT_SRC)
add_library(implot ${IMPLOT_SRC})
target_include_directories(implot PUBLIC implot imgui)
target_link_libraries(implot imgui)

add_subdirectory(jsoncpp EXCLUDE_FROM_ALL)
add_subdirectory(cpp-httplib EXCLUDE_FROM_ALL)
include_directories(cpp-httplib)

aux_source_directory(src IPIP_SRC)
add_executable(ipip ${IPIP_SRC})
target_link_libraries(ipip implot jsoncpp)

add_custom_target(run ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ipip DEPENDS ipip httplib::httplib)

install(TARGETS ipip RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})