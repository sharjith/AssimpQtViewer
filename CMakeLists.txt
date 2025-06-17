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
find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGLWidgets Widgets OpenGL Concurrent)

# Find OpenGL and Assimp
find_package(OpenGL REQUIRED)
find_package(assimp REQUIRED)

# Main executable
add_executable(AssimpQtViewer)


file(GLOB sources *.cpp *.cxx *.h *.hxx)
file(GLOB ui_sources *.ui)

qt_wrap_ui(ui_wrapped ${ui_sources})
qt_add_resources(resources Resource.qrc)

target_sources(AssimpQtViewer
    PRIVATE
        ${sources}
        ${ui_wrapped}
        ${resources}
)

target_include_directories(AssimpQtViewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}      
)

if(WIN32)
    target_sources(AssimpQtViewer
        PRIVATE
            icon.rc
    )   
endif()



# Link Qt modules and other dependencies
target_link_libraries(AssimpQtViewer PRIVATE
	Qt6::Core
    Qt6::Widgets
    Qt6::OpenGLWidgets
	Qt6::OpenGL
	Qt6::Widgets
    Qt6::Concurrent
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

if(WIN32)
    # Automatically detect the Qt bin folder (assumes Qt6::qmake is found)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    
    add_custom_command(TARGET AssimpQtViewer POST_BUILD
        COMMAND "${QT_BIN_DIR}/windeployqt.exe"
            --no-compiler-runtime
            --no-opengl-sw
            --no-translations
            "$<TARGET_FILE:AssimpQtViewer>"
        COMMENT "Running windeployqt to copy Qt dependencies"        
    )
endif()
