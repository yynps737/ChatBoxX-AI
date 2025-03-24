-- 初始化数据库

-- 用户表
CREATE TABLE users (
    id UUID PRIMARY KEY,
    username VARCHAR(64) NOT NULL UNIQUE,
    email VARCHAR(256) NOT NULL UNIQUE,
    password_hash VARCHAR(256) NOT NULL,
    salt VARCHAR(256) NOT NULL,
    is_active BOOLEAN DEFAULT TRUE,
    is_admin BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP NOT NULL DEFAULT NOW(),
    last_login_at TIMESTAMP
);

-- 对话表
CREATE TABLE dialogs (
    id UUID PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    title VARCHAR(128) NOT NULL,
    model_id VARCHAR(64) NOT NULL,
    is_archived BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP NOT NULL DEFAULT NOW()
);

-- 消息表
CREATE TABLE messages (
    id UUID PRIMARY KEY,
    dialog_id UUID NOT NULL REFERENCES dialogs(id) ON DELETE CASCADE,
    role VARCHAR(32) NOT NULL CHECK (role IN ('user', 'assistant', 'system')),
    content TEXT NOT NULL,
    type VARCHAR(32) NOT NULL DEFAULT 'text',
    tokens INTEGER DEFAULT 0,
    created_at TIMESTAMP NOT NULL DEFAULT NOW()
);

-- 文件表
CREATE TABLE files (
    id UUID PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    message_id UUID REFERENCES messages(id) ON DELETE SET NULL,
    name VARCHAR(256) NOT NULL,
    type VARCHAR(128) NOT NULL,
    size BIGINT NOT NULL,
    url VARCHAR(512) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT NOW()
);

-- 索引
CREATE INDEX idx_users_username ON users(username);
CREATE INDEX idx_users_email ON users(email);
CREATE INDEX idx_dialogs_user_id ON dialogs(user_id);
CREATE INDEX idx_dialogs_created_at ON dialogs(created_at);
CREATE INDEX idx_messages_dialog_id ON messages(dialog_id);
CREATE INDEX idx_messages_created_at ON messages(created_at);
CREATE INDEX idx_files_message_id ON files(message_id);
CREATE INDEX idx_files_user_id ON files(user_id);

-- 创建管理员用户（密码: admin123，SHA-256 哈希）
INSERT INTO users (
    id, 
    username, 
    email, 
    password_hash, 
    salt, 
    is_active, 
    is_admin, 
    created_at, 
    updated_at
) VALUES (
    '00000000-0000-0000-0000-000000000000',
    'admin',
    'admin@example.com',
    '8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918',
    'admin_salt',
    TRUE,
    TRUE,
    NOW(),
    NOW()
);