# CMake 最低版本要求
cmake_minimum_required(VERSION 3.22)

#
# 此文件由 STM32CubeMX 生成一次，
# 后续不会在转换时被重新覆盖。
# 用户可以根据需要自由修改此文件。
#

# ---- 编译器设置 ----
set(CMAKE_C_STANDARD 11)           # C语言标准：C11
set(CMAKE_C_STANDARD_REQUIRED ON)  # 强制要求C11标准
set(CMAKE_C_EXTENSIONS ON)         # 允许GNU扩展（如__attribute__）


# ---- 构建类型 ----
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")  # 默认Debug构建（含调试信息）
endif()

# ---- 项目名称 ----
set(CMAKE_PROJECT_NAME 3D_Hand_2)  # 项目名：灵巧手控制固件

# 生成 compile_commands.json，方便 clangd 等工具进行代码索引和补全
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# ---- 核心项目设置 ----
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# 启用 C 和 ASM（汇编）语言支持
enable_language(C ASM)

# 创建可执行文件目标（.elf）
add_executable(${CMAKE_PROJECT_NAME})

# ---- 添加 STM32CubeMX 生成的源文件 ----
add_subdirectory(cmake/stm32cubemx)   # 包含CubeMX生成的CMake子目录（含HAL库源码）

# ---- 链接目录设置 ----
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # 在此添加用户自定义的库搜索路径
)

# ---- 添加用户源文件 ----
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # 在此添加用户自定义的源文件
)

# ---- 添加用户头文件路径 ----
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # 在此添加用户自定义的头文件路径
)

# ---- 添加项目宏定义 ----
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # 在此添加用户自定义的预处理器宏
    # 例如：-DUSE_FULL_ASSERT
)

# 移除错误的 libob.a 库依赖（使用 C++ 文件时的兼容性修复）
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# ---- 链接库 ----
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx   # 链接CubeMX生成的HAL库目标

    # 在此添加用户自定义的库
)

# ---- 构建后处理：生成 .hex 和 .bin 烧录文件 ----
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
    COMMENT "生成 ${CMAKE_PROJECT_NAME}.hex 和 ${CMAKE_PROJECT_NAME}.bin 烧录文件"
)
