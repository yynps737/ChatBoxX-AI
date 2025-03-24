#!/bin/bash
set -e

# 等待 PostgreSQL 准备就绪
echo "等待 PostgreSQL..."
until pg_isready -h postgres -p 5432 -U ai_service; do
  echo "PostgreSQL 未就绪 - 等待..."
  sleep 2
done
echo "PostgreSQL 已准备就绪"

# 运行应用
exec "$@"
