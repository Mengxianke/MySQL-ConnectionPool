#include "query_result.h"
#include <sstream>

// =========================
// 构造和析构函数
// =========================

QueryResult::QueryResult(MYSQL_RES* result, unsigned long long affectedRows)
    : m_result(result)
    , m_currentRow(nullptr)
    , m_lengths(nullptr)
    , m_fieldCount(0)
    , m_rowCount(0)
    , m_affectedRows(affectedRows)
{
    // 如果有结果集，初始化元数据
    if (m_result) {
        initializeMetadata();
        LOG_DEBUG("QueryResult created with " + std::to_string(m_rowCount) + 
                  " rows and " + std::to_string(m_fieldCount) + " fields");
    } else {
        LOG_DEBUG("QueryResult created for non-SELECT operation, affected rows: " + 
                  std::to_string(m_affectedRows));
    }
}

QueryResult::~QueryResult() {
    if (m_result) {
        mysql_free_result(m_result);
        m_result = nullptr;
        LOG_DEBUG("QueryResult destroyed, MySQL result freed");
    }
}

// 移动构造函数
QueryResult::QueryResult(QueryResult&& other) noexcept
    : m_result(other.m_result)
    , m_currentRow(other.m_currentRow)
    , m_lengths(other.m_lengths)
    , m_fieldCount(other.m_fieldCount)
    , m_rowCount(other.m_rowCount)
    , m_affectedRows(other.m_affectedRows)
    , m_fieldNames(std::move(other.m_fieldNames))
{
    // 清空源对象，避免重复释放
    other.m_result = nullptr;
    other.m_currentRow = nullptr;
    other.m_lengths = nullptr;
    other.m_fieldCount = 0;
    other.m_rowCount = 0;
    other.m_affectedRows = 0;
}

// 移动赋值操作符
QueryResult& QueryResult::operator=(QueryResult&& other) noexcept {
    if (this != &other) {
        // 释放当前资源
        if (m_result) {
            mysql_free_result(m_result);
        }

        // 移动资源
        m_result = other.m_result;
        m_currentRow = other.m_currentRow;
        m_lengths = other.m_lengths;
        m_fieldCount = other.m_fieldCount;
        m_rowCount = other.m_rowCount;
        m_affectedRows = other.m_affectedRows;
        m_fieldNames = std::move(other.m_fieldNames);

        // 清空源对象
        other.m_result = nullptr;
        other.m_currentRow = nullptr;
        other.m_lengths = nullptr;
        other.m_fieldCount = 0;
        other.m_rowCount = 0;
        other.m_affectedRows = 0;
    }
    return *this;
}

// =========================
// 初始化方法
// =========================

void QueryResult::initializeMetadata() {
    if (!m_result) {
        return;
    }

    // 获取基本信息
    m_fieldCount = mysql_num_fields(m_result);
    m_rowCount = mysql_num_rows(m_result);

    // 获取字段名
    MYSQL_FIELD* fields = mysql_fetch_fields(m_result);
    m_fieldNames.clear();
    m_fieldNames.reserve(m_fieldCount);
    
    for (unsigned int i = 0; i < m_fieldCount; ++i) {
        m_fieldNames.push_back(fields[i].name);
    }

    LOG_DEBUG("QueryResult initialized: " + std::to_string(m_fieldCount) + 
              " fields, " + std::to_string(m_rowCount) + " rows");
}

// =========================
// 结果集导航方法
// =========================

bool QueryResult::next() {
    if (!m_result) {
        return false;
    }

    m_currentRow = mysql_fetch_row(m_result);
    if (m_currentRow) {
        m_lengths = mysql_fetch_lengths(m_result);
        return true;
    }

    return false;
}

bool QueryResult::reset() {
    if (!m_result) {
        return false;
    }

    // 重置到结果集开始位置
    mysql_data_seek(m_result, 0);
    m_currentRow = nullptr;
    m_lengths = nullptr;
    return true;
}

// =========================
// 元数据获取方法
// =========================

unsigned int QueryResult::getFieldCount() const {
    return m_fieldCount;
}

unsigned long long QueryResult::getRowCount() const {
    return m_rowCount;
}

unsigned long long QueryResult::getAffectedRows() const {
    return m_affectedRows;
}

std::vector<std::string> QueryResult::getFieldNames() const {
    return m_fieldNames;
}

bool QueryResult::isEmpty() const {
    return m_rowCount == 0;
}

bool QueryResult::hasResultSet() const {
    return m_result != nullptr;
}

// =========================
// 数据访问方法（按索引）
// =========================

std::string QueryResult::getString(unsigned int index) const {
    checkIndex(index);
    checkRow();

    if (m_currentRow[index] == nullptr) {
        return "";  // NULL值返回空字符串
    }

    return std::string(m_currentRow[index], m_lengths[index]);
}

int QueryResult::getInt(unsigned int index) const {
    checkIndex(index);
    checkRow();

    if (m_currentRow[index] == nullptr) {
        return 0;  // NULL值返回0
    }
    return safeConvert(m_currentRow[index], 0);
}

long long QueryResult::getLong(unsigned int index) const {
    checkIndex(index);
    checkRow();

    if (m_currentRow[index] == nullptr) {
        return 0LL;  // NULL值返回0
    }

    return safeConvert(m_currentRow[index], 0LL);
}

double QueryResult::getDouble(unsigned int index) const {
    checkIndex(index);
    checkRow();

    if (m_currentRow[index] == nullptr) {
        return 0.0;  // NULL值返回0.0
    }

    return safeConvert(m_currentRow[index], 0.0);
}

bool QueryResult::isNull(unsigned int index) const {
    checkIndex(index);
    checkRow();

    return m_currentRow[index] == nullptr;
}

// =========================
// 数据访问方法（按字段名）
// =========================

std::string QueryResult::getString(const std::string& fieldName) const {
    return getString(getFieldIndex(fieldName));
}

int QueryResult::getInt(const std::string& fieldName) const {
    return getInt(getFieldIndex(fieldName));
}

long long QueryResult::getLong(const std::string& fieldName) const {
    return getLong(getFieldIndex(fieldName));
}

double QueryResult::getDouble(const std::string& fieldName) const {
    return getDouble(getFieldIndex(fieldName));
}

bool QueryResult::isNull(const std::string& fieldName) const {
    return isNull(getFieldIndex(fieldName));
}

// =========================
// 私有辅助方法
// =========================

unsigned int QueryResult::getFieldIndex(const std::string& fieldName) const {
    for (unsigned int i = 0; i < m_fieldNames.size(); ++i) {
        if (m_fieldNames[i] == fieldName) {
            return i;
        }
    }

    throw std::out_of_range("Field name not found: " + fieldName);
}

void QueryResult::checkIndex(unsigned int index) const {
    if (index >= m_fieldCount) {
        throw std::out_of_range("Field index out of range: " + std::to_string(index) + 
                                ", max: " + std::to_string(m_fieldCount - 1));
    }
}

void QueryResult::checkRow() const {
    if (!m_currentRow) {
        throw std::runtime_error("No current row available. Call next() first.");
    }
}

int QueryResult::safeConvert(const char* value, int defaultValue) const {
    if (!value) {
        return defaultValue;
    }
    try {
        return std::stoi(value);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to convert '" + std::string(value) + "' to int: " + e.what());
        return defaultValue;
    }
}

long long QueryResult::safeConvert(const char* value, long long defaultValue) const {
    if (!value) {
        return defaultValue;
    }
    try {
        return std::stoll(value);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to convert '" + std::string(value) + "' to long long: " + e.what());
        return defaultValue;
    }
}

double QueryResult::safeConvert(const char* value, double defaultValue) const {
    if (!value) {
        return defaultValue;
    }
    try {
        return std::stod(value);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to convert '" + std::string(value) + "' to double: " + e.what());
        return defaultValue;
    }
}
#if 0
// =========================
// 模板特化：安全类型转换
// =========================

template<>
int QueryResult::safeConvert<int>(const char* value, int defaultValue) const {
    if (!value) {
        return defaultValue;
    }

    try {
        return std::stoi(value);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to convert '" + std::string(value) + "' to int: " + e.what());
        return defaultValue;
    }
}

template<>
long long QueryResult::safeConvert<long long>(const char* value, long long defaultValue) const {
    if (!value) {
        return defaultValue;
    }

    try {
        return std::stoll(value);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to convert '" + std::string(value) + "' to long long: " + e.what());
        return defaultValue;
    }
}

template<>
double QueryResult::safeConvert<double>(const char* value, double defaultValue) const {
    if (!value) {
        return defaultValue;
    }

    try {
        return std::stod(value);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to convert '" + std::string(value) + "' to double: " + e.what());
        return defaultValue;
    }
}

#endif