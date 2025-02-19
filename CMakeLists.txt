cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

# 设置项目名称和版本
project(luev VERSION 1.0 LANGUAGES C CXX)

# 设置 C 和 C++ 标准
set(CMAKE_C_STANDARD 17) # 设置 C 为 C17
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 让 C 代码支持 POSIX 和 GNU 扩展
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE")


file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/src/*.c")

# 添加编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -g -O0)
    add_compile_options(-Wno-unused-variable -Wno-unused-function -Wno-deprecated-declarations)
    add_compile_options(-Wno-unused-parameter)
    add_compile_options(-fanalyzer)

    #add_compile_definitions(EVENT_SHOW_METHOD=1)
    add_compile_definitions(USE_DEBUG)
endif()

# 创建可执行文件
add_executable(luev ${SOURCES})


# 添加头文件搜索路径
target_include_directories(luev PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/compat
)

# 链接 pthread 库(针对 UNIX)
if(UNIX)
    target_link_libraries(luev pthread)
endif()

# 设置默认的构建类型为 Debug（如果没有指定）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

# 设置调试信息并启用 AddressSanitizer 和 UndefinedBehaviorSanitizer
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer")
#set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer")


# 为 Release 构建类型设置优化标志
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -DNDEBUG")
