#ifndef DB_EXCEPTION_H
#define DB_EXCEPTION_H

#include <stdexcept>
#include <string>

namespace db {

// 数据库操作基础异常类
class DatabaseError : public std::runtime_error {
public:
    explicit DatabaseError(const std::string& msg) 
        : std::runtime_error(msg) {}
};

// SQL执行异常类（携带错误码）
class SQLExecutionError : public DatabaseError {
private:
    unsigned int m_errorCode;

public:
    SQLExecutionError(const std::string& msg, unsigned int code) 
        : DatabaseError(msg), m_errorCode(code) {}

    unsigned int getErrorCode() const { return m_errorCode; }
};

} // namespace db

#endif // DB_EXCEPTION_H