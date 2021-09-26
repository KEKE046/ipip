cmake_minimum_required(VERSION 3.16)

project(ipip VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

find_package(OpenGL REQUIRED)
# find_package(glfw3 REQUIRED)

message(${CMAKE_SYSTEM_NAME})

set(IMGUI_BACKEND glfw opengl3)

add_subdirectory(glfw EXCLUDE_FROM_ALL)
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

add_subdirectory(jsoncpp EXCLUDE_FROM_ALL)
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
include_directories(jsoncpp/include)
endif()
add_subdirectory(cpp-httplib EXCLUDE_FROM_ALL)
include_directories(cpp-httplib)

aux_source_directory(bin2c BIN2C_SRC)
add_executable(bin2c ${BIN2C_SRC})

add_custom_command(OUTPUT help.c COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin2c ${PROJECT_SOURCE_DIR}/src/help.html help.c HELP_HTML DEPENDS bin2c)

aux_source_directory(src IPIP_SRC)
add_executable(ipip ${IPIP_SRC} help.c)

message(${CMAKE_CXX_FLAGS})
if(${MSVC})
    if(${CMAKE_BUILD_TYPE} MATCHES "Release")
        target_compile_options(ipip PRIVATE /O2)
        target_compile_options(implot PRIVATE /O2)
        target_compile_options(imgui PRIVATE /O2)
    endif()
else()
    target_compile_options(ipip PRIVATE -O3)
    target_compile_options(implot PRIVATE -O3)
    target_compile_options(imgui PRIVATE -O3)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
target_link_libraries(ipip PRIVATE implot jsoncpp_static)
else ()
target_link_libraries(ipip PRIVATE implot jsoncpp)
endif ()

add_custom_target(run ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ipip DEPENDS ipip httplib::httplib)

install(TARGETS ipip RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})