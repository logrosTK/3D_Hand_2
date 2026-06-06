# STM32CubeMX 自动生成的 CMake 子项目
# 包含 HAL 库源文件、启动文件、用户应用源文件的编译配置
# 生成 stm32cubemx 接口库（头文件路径+宏定义）和 STM32_Drivers 对象库
cmake_minimum_required(VERSION 3.22)
# Enable CMake support for ASM and C languages
enable_language(C ASM)
# STM32CubeMX 生成的预处理器宏定义
set(MX_Defines_Syms 
	USE_HAL_DRIVER      # 使用HAL驱动（非LL库）
	STM32F103xB         # 目标芯片型号：STM32F103C8Tx（128K Flash, 中密度）
    $<$<CONFIG:Debug>:DEBUG>   # Debug模式下定义DEBUG宏
)

# STM32CubeMX 生成的头文件搜索路径
set(MX_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Inc                              # 用户应用头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy  # HAL库旧版兼容头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Inc       # HAL库头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Device/ST/STM32F1xx/Include  # CMSIS设备头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Include                   # CMSIS核心头文件
)

# STM32CubeMX 生成的用户应用源文件
set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/main.c               # 主程序和灵巧手控制逻辑
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/gpio.c               # GPIO初始化
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32f1xx_it.c       # 中断服务函数
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32f1xx_hal_msp.c  # HAL MSP初始化
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sysmem.c             # 堆内存管理（_sbrk）
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/syscalls.c           # 系统调用（_write/_read等）
    ${CMAKE_CURRENT_SOURCE_DIR}/../../startup_stm32f103xb.s         # 启动汇编文件
)

# STM32 HAL/LL 驱动库源文件
set(STM32_Drivers_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/system_stm32f1xx.c          # 系统初始化
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c         # HAL核心
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c     # 时钟配置
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c  # 时钟扩展
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c    # GPIO驱动
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c     # DMA驱动
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c  # Cortex内核
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c     # 电源管理
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c   # Flash驱动
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c    # 外部中断
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c     # 定时器驱动
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c  # 定时器扩展
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c    # 串口驱动
)

# Drivers Midllewares



# Link directories setup
set(MX_LINK_DIRS

)
# Project static libraries
set(MX_LINK_LIBS 
    STM32_Drivers
    ${TOOLCHAIN_LINK_LIBRARIES}
    
)
# 接口库：仅传递头文件路径和宏定义，不编译源文件
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

# 创建 STM32_Drivers 静态对象库（编译HAL驱动源文件为.o）
add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE ${STM32_Drivers_Src})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)


# 将应用源文件添加到主可执行目标
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src})

# 链接目录设置
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})

# 链接库
target_link_libraries(${CMAKE_PROJECT_NAME} ${MX_LINK_LIBS})

# 将.map文件添加到clean目标的清理列表
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# 验证C语言标准：STM32CubeMX生成的代码需要C11或更高版本
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
