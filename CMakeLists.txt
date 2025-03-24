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

# 寻找依赖库
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(PostgreSQL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json 3.10.0 REQUIRED)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)
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
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Threads::Threads
)

# 单元测试
enable_testing()
file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
add_executable(ai_backend_tests ${TEST_SOURCES})
target_link_libraries(ai_backend_tests
    PRIVATE
    ${Boost_LIBRARIES}
    ${PostgreSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    GTest::GTest
    GTest::Main
    Threads::Threads
)

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