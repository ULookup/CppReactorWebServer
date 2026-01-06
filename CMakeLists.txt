# 1. 添加 cmake 版本说明
cmake_minimum_required(VERSION 3.1.3)
# 2. 声明工程名称
project(webserver)

set(target "webserver")

# 4. 获取源码目录下的所有源码文件
set(src_files "")
set(http_files "")
set(log_files "")
set(util_files "")
set(replace_files, "")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src src_files)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/http http_files)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/log log_files)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/util util_files)
# 5. 声明目标及依赖
add_executable(${target} ${src_files} ${http_files} ${log_files} ${util_files} main.cc)

target_link_libraries(${target} -lspdlog -lfmt)

# 6. 设置头文件默认搜索路径
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/http)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/log)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/net)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/util)
# 7. 设置需要连接的库
# 8. 设置安装路径
INSTALL(TARGETS ${target} RUNTIME DESTINATION bin)
