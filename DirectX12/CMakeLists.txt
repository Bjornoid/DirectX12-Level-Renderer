cmake_minimum_required(VERSION 3.16)

project(Level_Renderer_D3D12)

# CMake FXC shader compilation, add any shaders you want compiled here
set(VERTEX_SHADERS 
	# add vertex shader (.hlsl) files here
	Shaders/VertexShader.hlsl
)

set(PIXEL_SHADERS 
	# add pixel shader (.hlsl) files here
	Shaders/PixelShader.hlsl
)

# Add any new C/C++ source code here
set(SOURCE_CODE
	# Header & CPP files go here
	main.cpp
	renderer.h
	FileIntoString.h
	h2bParser.h
	lvlData.h
	CameraMovement.h
)

if(WIN32)
# by default CMake selects "ALL_BUILD" as the startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
	PROPERTY VS_STARTUP_PROJECT Level_Renderer_D3D12)

	#this will tell Visual Studio to put the shaders under their own group in the solution explorer 
	set_property(GLOBAL PROPERTY USE_FOLDERS ON)
   	source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${VERTEX_SHADERS})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PIXEL_SHADERS})
endif()

# currently using unicode in some libraries on win32 but will change soon
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

add_executable (Level_Renderer_D3D12 
	${SOURCE_CODE}
	${VERTEX_SHADERS}
	${PIXEL_SHADERS}
)

set_source_files_properties( ${VERTEX_SHADERS} PROPERTIES 
        VS_SHADER_TYPE Vertex 
        VS_SHADER_MODEL 5.1
        VS_SHADER_ENTRYPOINT main
        VS_TOOL_OVERRIDE "FXCompile" 
)

set_source_files_properties( ${PIXEL_SHADERS} PROPERTIES 
	    VS_SHADER_TYPE Pixel 
        VS_SHADER_MODEL 5.1
        VS_SHADER_ENTRYPOINT main
        VS_TOOL_OVERRIDE "FXCompile"
)

# add support for dds texture loading
include_directories(../directxtk12/include)
# find the libraries
find_library(DDS_LIB_D NAMES DirectXTK12_x64_Debug
PATHS ../directxtk12/lib/)
find_library(DDS_LIB_R NAMES DirectXTK12_x64_Release
PATHS ../directxtk12/lib/)
# link the kitx sdk include and lib files
target_link_libraries(Level_Renderer_D3D12 debug ${DDS_LIB_D} optimized ${DDS_LIB_R})