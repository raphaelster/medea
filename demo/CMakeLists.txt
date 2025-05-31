cmake_minimum_required(VERSION 3.11.0)


project(medea-demo VERSION 0.1.0 LANGUAGES C CXX)

add_compile_options(-std=c++20 -Werror=return-type -mavx2 ${COMPILER_EXTRA_FLAGS})


link_libraries(-lglfw3 -lvulkan -lshaderc_combined)

file(GLOB_RECURSE sourceFiles CONFIGURE_DEPENDS engine/**.cpp engine/**.c)
list(APPEND sourceFiles main.cpp)
list(APPEND sourceFiles imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_tables.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_vulkan.cpp)
list(APPEND sourceFiles stb.cpp VkBootstrap.cpp vma_impl.cpp)

list(REMOVE_DUPLICATES sourceFiles)

add_executable(medea-demo ${sourceFiles})


target_include_directories(medea-demo PUBLIC "." "~/mylib/" "./imgui/" "./engine/math/" "./engine/" "~/vksdk/1.3.290.0/x86_64/include/")
target_link_directories(medea-demo PUBLIC "~/vksdk/1.3.290.0/x86_64/lib/")

add_custom_target(do_always ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}/shader ${CMAKE_BINARY_DIR}/shader
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}/textures ${CMAKE_BINARY_DIR}/textures)


include(CTest)
enable_testing()

