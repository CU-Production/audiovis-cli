cmake_minimum_required(VERSION 3.25)
project(audiovis_cli)

set(CMAKE_CXX_STANDARD 20)

# LabSound
add_subdirectory(3rd_party/LabSound)

# FTXUI
option(FTXUI_BUILD_DOCS OFF)
option(FTXUI_BUILD_EXAMPLES OFF)
option(FTXUI_ENABLE_INSTALL OFF)
add_subdirectory(3rd_party/FTXUI)

add_executable(audiovis_cli main.cpp)

if(CMAKE_HOST_APPLE)
        target_link_libraries(audiovis_cli PRIVATE
                ftxui::screen
                ftxui::dom
                ftxui::component
                LabSound
                "-framework CoreFoundation"
                "-framework CoreAudio"
                "-framework Accelerate"
        )
else()
        target_link_libraries(audiovis_cli PRIVATE
                ftxui::screen
                ftxui::dom
                ftxui::component
                LabSound
        )
endif()
