cmake_minimum_required(VERSION 3.15)

project(R2)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
file(GLOB r2src ./Source/**/**.cpp ./Source/**.cpp ./Source/VK/**.c)

find_package(Vulkan REQUIRED)

add_library(${PROJECT_NAME} STATIC ${r2src})

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_WIN32_KHR)
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC VK_NO_PROTOTYPES)

target_include_directories(${PROJECT_NAME} PRIVATE ./PrivateInclude ${Vulkan_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PUBLIC ./PublicInclude)