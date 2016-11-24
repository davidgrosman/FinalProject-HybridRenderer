cmake_minimum_required(VERSION 3.0)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

set(NAME HybridRenderer)

project(${NAME})

# Set up include and lib paths
set(EXTERNAL "external")
include_directories("${EXTERNAL}")
include_directories("${EXTERNAL}/include/")

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(EXTERNAL_LIB_PATH "${EXTERNAL}/lib/osx")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(EXTERNAL_LIB_PATH "${EXTERNAL}/lib/linux" "/usr/lib64")
elseif(WIN32)
    set(EXTERNAL_LIB_PATH "${EXTERNAL}/lib/win")
endif()
link_directories(${EXTERNAL_LIB_PATH})
list(APPEND CMAKE_LIBRARY_PATH "${EXTERNAL_LIB_PATH}")


# Find up and set up core dependency libs

set(GLFW_INCLUDE_DIR "external/include")
set(GLFW_LIBRARY_DIR "${EXTERNAL_LIB_PATH}")
set(GLFW_LIBRARY "../${EXTERNAL_LIB_PATH}/GLFW/glfw3")

set(GLEW_INCLUDE_DIR "external/include")
set(GLEW_LIBRARY_DIR "${EXTERNAL_LIB_PATH}")
add_definitions(-DGLEW_STATIC)
set(GLEW_LIBRARY "../${EXTERNAL_LIB_PATH}/GL/glew32s") #FIND_LIBRARY(GLEW_LIBRARY NAMES "glew32s" PATHS "${EXTERNAL_LIB_PATH}/GL" NO_DEFAULT_PATH)

FIND_LIBRARY(ASSIMP_LIBRARY NAMES "assimp" PATHS "${EXTERNAL_LIB_PATH}/ASSIMP" NO_DEFAULT_PATH)

find_package(OpenGL)

set(CORELIBS
    "${GLFW_LIBRARY}"
    "${OPENGL_LIBRARY}"
    "${GLEW_LIBRARY}"
    "${ASSIMP_LIBRARY}"
    )

OPTION(USE_D2D_WSI "Build the project using Direct to Display swapchain" OFF)

IF(WIN32)
	find_library(VULKAN_LIB NAMES vulkan-1 vulkan PATHS ${CMAKE_SOURCE_DIR}/libs/vulkan)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
ELSE(WIN32)
	find_library(VULKAN_LIB NAMES vulkan HINTS "$ENV{VULKAN_SDK}/lib" "${CMAKE_SOURCE_DIR}/libs/vulkan" REQUIRED)
	find_package(Threads REQUIRED)
IF(USE_D2D_WSI)
	MESSAGE("Using direct to display extension...")
	add_definitions(-D_DIRECT2DISPLAY)
ELSE(USE_D2D_WSI)
  	find_package(XCB REQUIRED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_XCB_KHR")
ENDIF(USE_D2D_WSI)
	# Todo : android?
ENDIF(WIN32)

message(STATUS ${VULKAN_LIB})

# Set preprocessor defines
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -D_USE_MATH_DEFINES")

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-std=c++11)

# create a list of files to copy
set( THIRD_PARTY_DLLS "../${EXTERNAL_LIB_PATH}/ASSIMP/assimp-vc140-mt.dll")

file(GLOB SOURCE *.cpp code/*.cpp *.h code/*.h)

# Function for building single example
function(buildExample EXAMPLE_NAME)
	file(GLOB SOURCE *.cpp code/*.cpp *.h code/*.h)
	if(WIN32)
		add_executable(${EXAMPLE_NAME} WIN32 ${SOURCE})
		target_link_libraries(${EXAMPLE_NAME} ${CORELIBS} ${VULKAN_LIB} ${ASSIMP_LIBRARIES} ${WINLIBS})
        
        set_target_properties(${EXAMPLE_NAME} PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
        set_target_properties(${EXAMPLE_NAME} PROPERTIES COMPILE_DEFINITIONS_DEBUG "_CONSOLE")
        set_target_properties(${EXAMPLE_NAME} PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE")
        set_target_properties(${EXAMPLE_NAME} PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO "_CONSOLE")
        set_target_properties(${EXAMPLE_NAME} PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE")
        set_target_properties(${EXAMPLE_NAME} PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE")
   
	else(WIN32)
		add_executable(${EXAMPLE_NAME} ${SOURCE})
		target_link_libraries(${EXAMPLE_NAME} ${CORELIBS} ${VULKAN_LIB} ${ASSIMP_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
	endif(WIN32)
    
    # do the copying
    foreach( file_i ${THIRD_PARTY_DLLS})
        add_custom_command(TARGET ${EXAMPLE_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy ${file_i} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/")
    endforeach( file_i )
endfunction(buildExample)

# Build all examples
function(buildExamples)
	foreach(EXAMPLE ${EXAMPLES})
		buildExample(${EXAMPLE})
	endforeach(EXAMPLE)
endfunction(buildExamples)

# Compiler specific stuff
IF(MSVC)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
ENDIF(MSVC)

IF(WIN32)
	# Nothing here (yet)
ELSE(WIN32)
	link_libraries(${XCB_LIBRARIES} ${VULKAN_LIB})
ENDIF(WIN32)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

set(EXAMPLES HybridRenderer)

buildExamples()
