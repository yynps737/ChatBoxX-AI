cmake_minimum_required(VERSION 3.14)
project(ai_backend VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(MSVC)
    add_compile_options(/W4 /MP)
else()
    add_compile_options(-Wall -Wextra -pedantic -O3)
endif()

# PostgreSQL路径设置
if(NOT PostgreSQL_TYPE_INCLUDE_DIR)
    set(PostgreSQL_TYPE_INCLUDE_DIR "/usr/include/postgresql")
endif()
if(NOT PostgreSQL_INCLUDE_DIR)
    set(PostgreSQL_INCLUDE_DIR "/usr/include/postgresql")
endif()

# 设置CMake模块路径，方便找到自定义的Find*.cmake文件
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

# 寻找依赖库
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(PostgreSQL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

# 尝试使用不同的方式查找nlohmann_json
include(ExternalProject)
if(NOT DEFINED nlohmann_json_FOUND)
    find_package(nlohmann_json 3.10.0 QUIET)
    if(NOT nlohmann_json_FOUND)
        # 如果找不到库，则内部使用包含的版本
        message(STATUS "nlohmann_json not found, using bundled version")
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/nlohmann)
        add_definitions(-DUSE_BUNDLED_NLOHMANN_JSON)
    endif()
endif()

# 尝试查找spdlog
if(NOT DEFINED spdlog_FOUND)
    find_package(spdlog QUIET)
    if(NOT spdlog_FOUND)
        # 如果找不到库，则内部使用包含的版本
        message(STATUS "spdlog not found, using bundled version")
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/spdlog)
        add_definitions(-DUSE_BUNDLED_SPDLOG)
    endif()
endif()

# 尝试查找GTest
if(NOT DEFINED GTest_FOUND)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        # 如果找不到库，则内部使用包含的版本或禁用测试
        message(STATUS "GTest not found, disabling tests")
        set(ENABLE_TESTS OFF)
    else()
        set(ENABLE_TESTS ON)
    endif()
else()
    set(ENABLE_TESTS ON)
endif()

find_package(Threads REQUIRED)

# 源文件集合
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${PostgreSQL_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${CURL_INCLUDE_DIRS})

# 主可执行文件
add_executable(ai_backend ${SOURCES})

# 链接库
target_link_libraries(ai_backend
    PRIVATE
    ${Boost_LIBRARIES}
    ${PostgreSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    Threads::Threads
)

# 添加nlohmann_json库
if(nlohmann_json_FOUND)
    target_link_libraries(ai_backend PRIVATE nlohmann_json::nlohmann_json)
endif()

# 添加spdlog库
if(spdlog_FOUND)
    target_link_libraries(ai_backend PRIVATE spdlog::spdlog)
endif()

# 单元测试
if(ENABLE_TESTS)
    enable_testing()
    file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
    add_executable(ai_backend_tests ${TEST_SOURCES})
    target_link_libraries(ai_backend_tests
        PRIVATE
        ${Boost_LIBRARIES}
        ${PostgreSQL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${CURL_LIBRARIES}
        Threads::Threads
    )
    
    if(nlohmann_json_FOUND)
        target_link_libraries(ai_backend_tests PRIVATE nlohmann_json::nlohmann_json)
    endif()
    
    if(spdlog_FOUND)
        target_link_libraries(ai_backend_tests PRIVATE spdlog::spdlog)
    endif()
    
    if(GTest_FOUND)
        target_link_libraries(ai_backend_tests PRIVATE GTest::GTest GTest::Main)
    endif()
endif()

# 安装配置
install(TARGETS ai_backend
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/config/
    DESTINATION etc/ai_backend
    FILES_MATCHING PATTERN "*.toml"
)

# 拷贝配置文件到构建目录
file(COPY ${PROJECT_SOURCE_DIR}/config/ DESTINATION ${CMAKE_BINARY_DIR}/config/)