cmake_minimum_required(VERSION 3.10)
project(gtkwave_cpp_clone VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 5.9 COMPONENTS Core Widgets REQUIRED)

set(SOURCES
    src/main.cpp
    src/ui/main_window.cpp
    src/ui/signal_tree.cpp
    src/ui/waveform_view.cpp
    src/fst/simple_fst_reader.cpp
)

set(HEADERS
    include/main_window.h
    include/signal_tree.h
    include/waveform_view.h
    include/simple_fst_reader.h
)

add_executable(gtkwave_cpp_clone
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(gtkwave_cpp_clone
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gtkwave_cpp_clone
    PRIVATE
        Qt5::Core
        Qt5::Widgets
)

# Enable automoc for Qt signals/slots
set_target_properties(gtkwave_cpp_clone PROPERTIES
    AUTOMOC ON
    AUTOUIC OFF
    AUTORCC OFF
)

install(TARGETS gtkwave_cpp_clone RUNTIME DESTINATION bin)
