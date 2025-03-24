FROM ubuntu:22.04 as builder

# 设置时区
ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# 安装编译依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libpq-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    pkg-config \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# 拷贝源代码
WORKDIR /src
COPY . .

# 构建项目
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . -- -j$(nproc)

FROM ubuntu:22.04

# 设置时区
ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# 安装运行时依赖
RUN apt-get update && apt-get install -y \
    libpq5 \
    libssl3 \
    libcurl4 \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libboost-filesystem1.74.0 \
    && rm -rf /var/lib/apt/lists/*

# 创建应用目录
WORKDIR /app
RUN mkdir -p /app/uploads

# 复制构建产物
COPY --from=builder /src/build/ai_backend /app/
COPY --from=builder /src/config /app/config

# 创建非特权用户
RUN useradd -m -u 1000 -s /bin/bash appuser && \
    chown -R appuser:appuser /app

# 切换到非特权用户
USER appuser

# 暴露端口
EXPOSE 8080

# 设置健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/api/v1/health || exit 1

# 运行应用
CMD ["/app/ai_backend", "/app/config/config.toml"]