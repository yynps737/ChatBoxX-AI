FROM ubuntu:22.04 AS builder

# 设置时区
ENV TZ=UTC
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# 安装构建依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    wget \
    git \
    libboost-all-dev \
    libpqxx-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    pkg-config \
    nlohmann-json3-dev \
    libspdlog-dev \
    libgtest-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# 安装 CMake 3.14+ (项目要求)
RUN wget https://github.com/Kitware/CMake/releases/download/v3.20.0/cmake-3.20.0-Linux-x86_64.sh \
    -q -O /tmp/cmake-install.sh \
    && chmod u+x /tmp/cmake-install.sh \
    && mkdir -p /opt/cmake \
    && /tmp/cmake-install.sh --skip-license --prefix=/opt/cmake \
    && rm /tmp/cmake-install.sh \
    && ln -s /opt/cmake/bin/* /usr/local/bin

# 创建构建目录
WORKDIR /app

# 复制源代码
COPY . .

# 创建测试目录结构
RUN mkdir -p include/third_party/nlohmann && \
    mkdir -p include/third_party/spdlog

# 修改源代码文件以解决命名空间问题
RUN find src -name "*.cpp" -exec sed -i 's/Task<Response>/core::async::Task<core::http::Response>/g' {} \; && \
    find src -name "*.cpp" -exec sed -i 's/Response::/core::http::Response::/g' {} \; && \
    find src -name "*.cpp" -exec sed -i 's/Request/core::http::Request/g' {} \; && \
    find include -name "*.h" -exec sed -i 's/spdlog::error(\(.*\), \(.*\))/spdlog::error(fmt::format(\1, \2))/g' {} \;

# 创建构建目录并禁用测试构建
RUN mkdir -p build && cd build && \
    cmake -DENABLE_TESTS=OFF .. && \
    make -j$(nproc)

# 运行时镜像
FROM ubuntu:22.04

# 设置时区
ENV TZ=UTC
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# 安装运行时依赖和工具
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libpqxx-6.4 \
    libssl3 \
    libcurl4 \
    libspdlog1 \
    postgresql-client \
    curl \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# 创建应用目录
WORKDIR /app

# 从构建阶段复制二进制文件和配置文件
COPY --from=builder /app/build/ai_backend /app/
COPY --from=builder /app/config /app/config/

# 复制启动脚本
COPY docker-entrypoint.sh /app/
RUN chmod +x /app/docker-entrypoint.sh

# 创建上传目录
RUN mkdir -p /app/uploads && chmod 755 /app/uploads

# 暴露端口
EXPOSE 8080

# 设置入口点
ENTRYPOINT ["/app/docker-entrypoint.sh"]

# 运行应用的命令
CMD ["/app/ai_backend", "/app/config/config.toml"]
