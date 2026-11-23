cmake_minimum_required(VERSION 3.16)
project(PwAG_RPG LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# freeglut budowany ze zrodel razem z gra - nikt nie musi go instalowac
set(FREEGLUT_BUILD_DEMOS       OFF CACHE BOOL "" FORCE)
set(FREEGLUT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(FREEGLUT_BUILD_STATIC_LIBS ON  CACHE BOOL "" FORCE)
set(FREEGLUT_REPLACE_GLUT      OFF CACHE BOOL "" FORCE)
set(INSTALL_PDB                OFF CACHE BOOL "" FORCE)
add_subdirectory(external/freeglut EXCLUDE_FROM_ALL)

find_package(OpenGL REQUIRED)

# GLOB, zeby nowy plik w src/ (inventory.cpp itd.) nie wymagal zmian w tym pliku
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
add_executable(gra ${GAME_SOURCES})

target_include_directories(gra PRIVATE external external/freeglut/include)
target_link_libraries(gra PRIVATE freeglut_static OpenGL::GL)

if(WIN32)
    # glext.h z labow (MSVC nie ma wlasnego); funkcje GL ladowane przez wglGetProcAddress.
    # FREEGLUT_LIB_PRAGMAS=0: biblioteke podaje CMake, naglowek freeglut ma jej sam nie dolaczac.
    # _CRT_SECURE_NO_WARNINGS, bo MSVC marudzi przy kazdym fopen i sscanf.
    target_include_directories(gra PRIVATE external/win32)
    target_compile_definitions(gra PRIVATE FREEGLUT_LIB_PRAGMAS=0 _CRT_SECURE_NO_WARNINGS)
else()
    # libGL na Linuxie eksportuje wszystkie funkcje OpenGL - wystarcza prototypy
    target_compile_definitions(gra PRIVATE GL_GLEXT_PROTOTYPES)
endif()

if(MSVC)
    target_compile_options(gra PRIVATE /utf-8 /W3)      # polskie napisy w UTF-8
else()
    target_compile_options(gra PRIVATE -Wall)
endif()
if(MINGW)
    target_link_options(gra PRIVATE -static)            # gra.exe bez dodatkowych DLL
endif()

# Zawsze bin/gra, takze w Visual Studio. "$<0:>" wylacza podkatalog Release/ - nie pytajcie, to dziala.
set_target_properties(gra PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<0:>")
