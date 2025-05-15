cmake_minimum_required(VERSION 3.16)
project(AssimpQtViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: Help locate Qt6 if not found automatically
# set(CMAKE_PREFIX_PATH "C:/Qt/6.5.2/msvc2019_64/lib/cmake")

# Required for moc/uic/rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt
find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGLWidgets Widgets OpenGL)

# Find OpenGL and Assimp
find_package(OpenGL REQUIRED)
find_package(assimp REQUIRED)

# Source files
set(SOURCES
    main.cpp
	MainWindow.cpp
    ModelViewerWidget.cpp    
)

set(HEADERS
	MainWindow.h
    ModelViewerWidget.h    
)

# Main executable
add_executable(AssimpQtViewer ${SOURCES} ${HEADERS})

# Link Qt modules and other dependencies
target_link_libraries(AssimpQtViewer PRIVATE
	Qt6::Core
    Qt6::Widgets
    Qt6::OpenGLWidgets
	Qt6::OpenGL
	Qt6::Widgets
    OpenGL::GL
    assimp::assimp
)

if(WIN32)
	target_link_libraries(AssimpQtViewer PRIVATE glu32)
elseif(UNIX)
	find_package(GLU REQUIRED)
	target_link_libraries(AssimpQtViewer PRIVATE OpenGL::GLU)
endif()

# This ensures include directories from linked Qt targets are added
target_include_directories(AssimpQtViewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
