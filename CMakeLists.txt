cmake_minimum_required(VERSION 3.16)

project(ipip VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(OpenGL REQUIRED)
# find_package(glfw3 REQUIRED)

message(${CMAKE_SYSTEM_NAME})

set(CMAKE_CXX_FLAGS "-O3")

set(IMGUI_BACKEND glfw opengl3)

add_subdirectory(glfw)
include_directories(glfw/include)

aux_source_directory(imgui IMGUI_SRC)
foreach(BAK IN ITEMS ${IMGUI_BACKEND})
    list(APPEND IMGUI_SRC imgui/backends/imgui_impl_${BAK}.cpp)
endforeach(BAK IN ${IMGUI_BACKEND})
add_library(imgui ${IMGUI_SRC})
target_include_directories(imgui PUBLIC imgui)
target_compile_definitions(imgui PUBLIC ImDrawIdx=unsigned)
target_link_libraries(imgui glfw ${OPENGL_LIBRARIES} ${CMAKE_DL_LIBS})

aux_source_directory(implot IMPLOT_SRC)
add_library(implot ${IMPLOT_SRC})
target_include_directories(implot PUBLIC implot imgui)
target_link_libraries(implot imgui)

add_subdirectory(jsoncpp)
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
include_directories(jsoncpp/include)
endif()
add_subdirectory(cpp-httplib)
include_directories(cpp-httplib)

aux_source_directory(bin2c BIN2C_SRC)
add_executable(bin2c ${BIN2C_SRC})

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
add_custom_command(OUTPUT help.c COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug/bin2c.exe ${PROJECT_SOURCE_DIR}/src/help.html help.c HELP_HTML DEPENDS bin2c)
else()
add_custom_command(OUTPUT help.c COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin2c ${PROJECT_SOURCE_DIR}/src/help.html help.c HELP_HTML DEPENDS bin2c)
endif()

aux_source_directory(src IPIP_SRC)
add_executable(ipip ${IPIP_SRC} help.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
target_link_libraries(ipip implot jsoncpp_static)
else ()
target_link_libraries(ipip implot jsoncpp)
endif ()

add_custom_target(run ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ipip DEPENDS ipip httplib::httplib)

install(TARGETS ipip RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})