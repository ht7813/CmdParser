// CLI Command Parser
// by ht7813
//
// SPDX-License-Identifier: MIT
#pragma once

#include <stdexcept>
#include <typeinfo>
#include <typeindex>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cmath>

// ============================================================================
// C++ Standard Detection
// ============================================================================

#if defined(_MSVC_LANG)
    #define __CMDPARSER_CPP_STANDARD _MSVC_LANG // Why Microsoft make special on everything ?!?!
#else
    #define __CMDPARSER_CPP_STANDARD __cplusplus
#endif

// ============================================================================
// Internal Helper Macros
// ============================================================================

// Internal Define, DO NOT Use in user program
#define __CMDPARSER_INTERNAL_CONVERTER_ITEM(T) {std::type_index(typeid(T)), convertFromString<T>},
// Internal Define, DO NOT Use in user program
#define __CMDPARSER_INTERNAL_DELETER_TOSTRING_ITEM(T) \
        else if (type_ == typeid(T)) { \
            deleter_ = [](void* p) { delete static_cast<T*>(p); }; \
            toString_ = [](const void* p) { \
                return std::to_string(*static_cast<const T*>(p)); \
            }; \
        }

// ============================================================================
// Debug Macros
// ============================================================================

#ifdef CMDPARSER_DEBUG
    #define CMDPARSER_DEBUG_PRINT(fmt, ...) \
        printf("[DEBUG] %s:%d: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define CMDPARSER_DEBUG_PRINT(fmt, ...) ((void)0)
#endif

namespace cmdparser {

// 前置声明
class CommandArgument;
class Command;
class CommandRegister;
class CommandParser;

// ============================================================================
// Internal Helpers
// ============================================================================
namespace __internal {
    bool endsWith(const std::string& str, const std::string& suffix) {
        #if __CMDPARSER_CPP_STANDARD >= 202002L
        return str.ends_with(suffix); // C++ 20
        #else
        if (suffix.size() > str.size()) return false;
        return str.rfind(suffix) == str.size() - suffix.size();
        #endif
    }

    bool isVariadicArg(const std::string& name) {
        return endsWith(name, "...");
    }
}

// ============================================================================
// 异常定义
// ============================================================================
namespace exceptions {

class InvalidCommandSyntax : public std::exception {
private:
    std::string message_;
public:
    explicit InvalidCommandSyntax(const std::string& msg) : message_(msg) {}
    explicit InvalidCommandSyntax(const char* msg) : message_(msg) {}
    const char* what() const noexcept override {
        return message_.c_str();
    }
};

class MissingRequiredArgument : public InvalidCommandSyntax {
public:
    explicit MissingRequiredArgument(const std::string& argName)
        : InvalidCommandSyntax("Missing required argument: " + argName) {}
};

class UnknownCommand : public InvalidCommandSyntax {
public:
    explicit UnknownCommand(const std::string& cmdName)
        : InvalidCommandSyntax("Unknown command: " + cmdName) {}
};

class DuplicateCommand : public InvalidCommandSyntax {
public:
    explicit DuplicateCommand(const std::string& cmdName)
        : InvalidCommandSyntax("Duplicate command: " + cmdName) {}
};

class DuplicateArgument : public InvalidCommandSyntax {
public:
    explicit DuplicateArgument(const std::string& cmdName)
        : InvalidCommandSyntax("Duplicate argument: " + cmdName) {}
};

class DuplicateArgumentAlias : public InvalidCommandSyntax {
public:
    explicit DuplicateArgumentAlias(const std::string& aliasName, const std::string& argName)
        : InvalidCommandSyntax("Duplicate alias " + aliasName + " for argument " + argName) {}
};

class ArgumentTypeMismatch : public InvalidCommandSyntax {
public:
    explicit ArgumentTypeMismatch(const std::string& argName)
        : InvalidCommandSyntax("Argument type mismatch: " + argName) {}
};

class ArgumentNotFound : public InvalidCommandSyntax {
public:
    explicit ArgumentNotFound(const std::string& argName)
        : InvalidCommandSyntax("Argument not found: " + argName) {}
};

} // namespace exceptions

// ============================================================================
// 类型转换辅助
// ============================================================================
namespace type_conversion {

template<typename T>
T parseValue(const std::string& str) {
    std::stringstream ss(str);
    T value;
    ss >> value;
    if (ss.fail() || !ss.eof()) {
        throw exceptions::InvalidCommandSyntax("Failed to convert '" + str + "' to requested type");
    }
    if constexpr (std::is_floating_point_v<T>) {
        if (std::isnan(value) || std::isinf(value)) {
            throw exceptions::InvalidCommandSyntax("Invalid floating point value: '" + str + "'");
        }
    }
    return value;
}

// 获取类型对应的默认值（用于解析失败时的回退）
template<typename T>
struct DefaultValue {
    static T get() { return T{}; }
};

template<>
struct DefaultValue<bool> {
    static bool get() { return false; }
};

template<>
struct DefaultValue<int> {
    static int get() { return 0; }
};

template<>
struct DefaultValue<long> {
    static long get() { return 0L; }
};

template<>
struct DefaultValue<float> {
    static float get() { return 0.0f; }
};

template<>
struct DefaultValue<double> {
    static double get() { return 0.0; }
};

template<>
struct DefaultValue<std::string> {
    static std::string get() { return ""; }
};

} // namespace type_conversion

// ============================================================================
// 类型擦除的值存储（用于支持任意类型参数）
// ============================================================================
class ArgumentValue {
private:
    std::type_index type_;
    void* data_;
    std::function<void(void*)> deleter_;
    std::function<std::string(const void*)> toString_;
    std::string rawString_;
    bool isLazy_ = false;

    template<typename T>
    static void* convertFromString(const std::string& str) {
        return new T(type_conversion::parseValue<T>(str));
    }

    // 类型转换函数表
    using ConverterFunc = void*(*)(const std::string&);
    static ConverterFunc getConverter(const std::type_index& type) {
        static std::unordered_map<std::type_index, ConverterFunc> converters = {
            __CMDPARSER_INTERNAL_CONVERTER_ITEM(int)
            __CMDPARSER_INTERNAL_CONVERTER_ITEM(long)
            __CMDPARSER_INTERNAL_CONVERTER_ITEM(long long)
            __CMDPARSER_INTERNAL_CONVERTER_ITEM(unsigned int)
            __CMDPARSER_INTERNAL_CONVERTER_ITEM(unsigned long)
            __CMDPARSER_INTERNAL_CONVERTER_ITEM(unsigned long long)
            __CMDPARSER_INTERNAL_CONVERTER_ITEM(float)
            __CMDPARSER_INTERNAL_CONVERTER_ITEM(double)
            __CMDPARSER_INTERNAL_CONVERTER_ITEM(long double)
            __CMDPARSER_INTERNAL_CONVERTER_ITEM(size_t)
            {std::type_index(typeid(std::string)), [](const std::string& str) -> void* {
                return new std::string(str);
            }},
        };
        auto it = converters.find(type);
        if (it != converters.end()) {
            return it->second;
        }
        return nullptr;
    }

    void setupDeleterAndToString() {
        if (type_ == typeid(std::string)) {
            deleter_ = [](void* p) { delete static_cast<std::string*>(p); };
            toString_ = [](const void* p) {
                return *static_cast<const std::string*>(p);
            };
        }
        __CMDPARSER_INTERNAL_DELETER_TOSTRING_ITEM(int)
        __CMDPARSER_INTERNAL_DELETER_TOSTRING_ITEM(long)
        __CMDPARSER_INTERNAL_DELETER_TOSTRING_ITEM(long long)
        __CMDPARSER_INTERNAL_DELETER_TOSTRING_ITEM(unsigned int)
        __CMDPARSER_INTERNAL_DELETER_TOSTRING_ITEM(unsigned long)
        __CMDPARSER_INTERNAL_DELETER_TOSTRING_ITEM(unsigned long long)
        __CMDPARSER_INTERNAL_DELETER_TOSTRING_ITEM(float)
        __CMDPARSER_INTERNAL_DELETER_TOSTRING_ITEM(double)
        __CMDPARSER_INTERNAL_DELETER_TOSTRING_ITEM(long double)
        __CMDPARSER_INTERNAL_DELETER_TOSTRING_ITEM(size_t)
    }

public:
    // 默认构造（空值）
    ArgumentValue() : type_(typeid(void)), data_(nullptr), deleter_(nullptr), toString_(nullptr), isLazy_(false) {}

    template<typename T>
    explicit ArgumentValue(const T& value)
        : type_(typeid(T))
        , data_(new T(value))
        , deleter_([](void* p) { delete static_cast<T*>(p); })
        , toString_([](const void* p) { 
            std::stringstream ss;
            ss << *static_cast<const T*>(p);
            return ss.str();
        })
        , isLazy_(false) {}
    
    explicit ArgumentValue(const std::string& str)
        : type_(typeid(std::string))  // 默认存储为字符串
        , data_(new std::string(str))
        , deleter_([](void* p) { delete static_cast<std::string*>(p); })
        , toString_([](const void* p) {
            return *static_cast<const std::string*>(p);
        })
        , rawString_(str)
        , isLazy_(true) {}

    // 新增：从字符串构造，指定目标类型（立即转换或延迟）
    ArgumentValue(const std::string& str, const std::type_index& targetType)
        : type_(targetType)
        , data_(nullptr)
        , deleter_(nullptr)
        , toString_(nullptr)
        , rawString_(str)
        , isLazy_(true) {
        // 尝试立即转换
        convertLazy();
    }

    ~ArgumentValue() {
        if (deleter_ && data_) {
            deleter_(data_);
        }
    }

    // 拷贝构造（深拷贝）
    ArgumentValue(const ArgumentValue& other)
        : type_(other.type_)
        , data_(nullptr)
        , deleter_(nullptr)
        , toString_(other.toString_) {
        if (other.data_ && other.deleter_) {
            // 需要根据类型拷贝数据
            // 由于无法在运行时知道类型，这里简化处理：
            // 使用 std::any 或者用虚函数，或者这里只支持移动
            // 更好的做法是使用 std::any（C++17）或 boost::any
            throw std::runtime_error("Copy not supported, use move instead");
        }
    }

    // 移动构造
    ArgumentValue(ArgumentValue&& other) noexcept
        : type_(std::move(other.type_))
        , data_(other.data_)
        , deleter_(std::move(other.deleter_))
        , toString_(std::move(other.toString_)) {
        other.data_ = nullptr;
        other.deleter_ = nullptr;
	other.isLazy_ = false;
    }

    // 拷贝赋值（删除）
    ArgumentValue& operator=(const ArgumentValue&) = delete;

    // 移动赋值
    ArgumentValue& operator=(ArgumentValue&& other) noexcept {
        if (this != &other) {
            // 释放当前资源
            if (deleter_ && data_) {
                deleter_(data_);
            }
            type_ = std::move(other.type_);
            data_ = other.data_;
            deleter_ = std::move(other.deleter_);
            toString_ = std::move(other.toString_);
            other.data_ = nullptr;
            other.deleter_ = nullptr;
	    other.isLazy_ = false;
        }
        return *this;
    }

    void convertLazy() {
        if (!isLazy_) {
            return;
        }

        // 如果已经转换过了，跳过
        if (data_ != nullptr && type_ != typeid(std::string)) {
            return;
        }

        auto converter = getConverter(type_);
        if (converter) {
            // 释放旧的字符串数据
            if (deleter_ && data_) {
                deleter_(data_);
            }
            // 执行转换
            data_ = converter(rawString_);
            // 设置对应的 deleter 和 toString
            setupDeleterAndToString();
            isLazy_ = false;
        }
    }

    template<typename T>
    const T& as() const {
        // 如果是延迟转换，先执行转换（const_cast 用于调用非 const 方法）
        if (isLazy_) {
            const_cast<ArgumentValue*>(this)->convertLazy();
        }
        if (type_ != typeid(T)) {
            throw exceptions::ArgumentTypeMismatch("Cannot cast argument to requested type");
        }
        return *static_cast<const T*>(data_);
    }

    std::string toString() const {
        if (isLazy_) {
            return rawString_;
        }
        if (toString_ && data_) {
            return toString_(data_);
        }
        return "";
    }

    std::type_index type() const { return type_; }
    bool isEmpty() const { return data_ == nullptr; }
    bool isLazy() const {return isLazy_; }

    void setTargetType(const std::type_info& type) {
	if (isLazy_ && type_ != type) {
	    type_ = type;
	    if (data_ && deleter_) {
		deleter_(data_);
		data_ = nullptr;
	    }
	    convertLazy();
	}
    }
};

// ============================================================================
// CommandArgument - 解析后的参数集合
// ============================================================================
class CommandArgument {
private:
    std::unordered_map<std::string, ArgumentValue> args_;
    std::vector<ArgumentValue> positionalArgs_; // 位置参数

public:
    CommandArgument() = default;

    void set(const std::string& name, const ArgumentValue& value) {
        // 删除旧值，插入新值
        args_.erase(name);
        args_.emplace(name, value);  // 这里会调用拷贝构造
        // 或者用 insert_or_assign (C++17)
        // args_.insert_or_assign(name, value);
    }

    // 移动版本
    void set(const std::string& name, ArgumentValue&& value) {
        args_.erase(name);
        args_.emplace(name, std::move(value));
    }

    template<typename T>
    void set(const std::string& name, const T& value) {
        args_.erase(name);
        args_.emplace(name, ArgumentValue(value));
    }

    void set(const std::string& name, const std::string& value) {
        args_.erase(name);
        args_.emplace(name, ArgumentValue(value));
    }


    void addPositional(ArgumentValue&& value) {
        positionalArgs_.push_back(std::move(value));
    }

    // 获取命名参数
    template<typename T>
    const T& get(const std::string& name) const {
        auto it = args_.find(name);
        if (it == args_.end()) {
            throw exceptions::ArgumentNotFound(name);
        }
        return it->second.as<T>();
    }

    // 获取位置参数
    template<typename T>
    const T& getPositional(size_t index) const {
        if (index >= positionalArgs_.size() || index < 0) {
            throw exceptions::InvalidCommandSyntax("Positional argument index out of range");
        }
        return positionalArgs_[index].as<T>();
    }

    // 检查是否存在
    bool has(const std::string& name) const {
        return args_.find(name) != args_.end();
    }

    size_t positionalCount() const { return positionalArgs_.size(); }

    // 获取所有参数名
    std::vector<std::string> getArgumentNames() const {
        std::vector<std::string> names;
        for (const auto& pair : args_) {
            names.push_back(pair.first);
        }
        return names;
    }

    // 获取参数类型名
    std::string getTypeName(const std::string& name) const {
        auto it = args_.find(name);
        if (it == args_.end()) {
            return "unknown";
        }
        return it->second.type().name();
    }

    // 调试输出
    void dump() const {
        printf("Arguments:\n");
        for (const auto& pair : args_) {
            printf("  %s = %s (type: %s)\n", 
                   pair.first.c_str(), 
                   pair.second.toString().c_str(),
                   pair.second.type().name());
        }
        printf("Positional:\n");
        for (size_t i = 0; i < positionalArgs_.size(); i++) {
            printf("  [%zu] = %s\n", i, positionalArgs_[i].toString().c_str());
        }
    }
};

// ============================================================================
// Command - 命令定义（内部存储）
// ============================================================================
class Command {
public:
    struct ArgumentDef {
        std::string name;
        std::type_index type;
        bool isPositional;
        bool isOptional;
        bool hasDefault;
        std::string defaultValue;
        std::string description;
        std::vector<std::string> aliases; // 可选：短选项别名
	
	ArgumentDef()
        : name("")
        , type(typeid(void))
        , isPositional(false)
        , isOptional(true), hasDefault(false) {}

        ArgumentDef(const std::string& n, const std::type_info& t, bool pos = false, bool opt = true)
            : name(n), type(t), isPositional(pos), isOptional(opt), hasDefault(false) {}
        
        void addAlias(const std::string& alias) {
            if (std::find(aliases.begin(), aliases.end(), alias) != aliases.end()) throw exceptions::DuplicateArgumentAlias(alias, name);
            aliases.push_back(alias);
        }
        void setDefault(const std::string& value) {
            defaultValue = value;
            hasDefault = true;
        }
        void setDescription(const std::string& value) {
            description = value;
        }
    };

private:
    std::string name_;
    std::string description_;
    std::vector<ArgumentDef> argDefs_;
    std::unordered_map<std::string, size_t> argIndex_; // 名称 -> 索引
    std::function<bool(CommandArgument&)> handler_;
    bool hasHandler_ = false;

public:
    Command(const std::string& name) : name_(name) {}

    void setDescription(const std::string& desc) { description_ = desc; }

    void addArgument(const std::string& name, const std::type_info& type, bool positional = false, bool optional = true) {
        if (argIndex_.find(name) != argIndex_.end()) {
            throw exceptions::DuplicateArgument(name);
        }
        
        // 2. 检查是否和其他参数的别名冲突
        for (const auto& existing : argDefs_) {
            for (const auto& alias : existing.aliases) {
                if (alias == name) {
                    throw exceptions::DuplicateArgument(
                        "Argument name '" + name + "' conflicts with alias of '" + 
                        existing.name + "'"
                    );
                }
            }
            if ((existing.isPositional && existing.isOptional) && (positional && !optional))
                throw exceptions::InvalidCommandSyntax("Required positional arguments cannot appear after optional positional arguments");
            if ((existing.isPositional && __internal::isVariadicArg(existing.name)) && positional)
                throw exceptions::InvalidCommandSyntax("New positional arguments cannot appear after long positional arguments (ends with ...)");
        }
        argIndex_[name] = argDefs_.size();
        argDefs_.emplace_back(name, type, positional, optional);
    }

    void setHandler(std::function<bool(CommandArgument&)> handler) {
        handler_ = handler;
        hasHandler_ = true;
    }

    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    std::vector<ArgumentDef>& getArguments2() { return argDefs_; }
    const std::vector<ArgumentDef>& getArguments() const { return argDefs_; }

    bool hasHandler() const { return hasHandler_; }

    // 执行命令
    bool execute(CommandArgument& args) const {
        if (!hasHandler_) {
            throw exceptions::InvalidCommandSyntax("Command has no handler: " + name_);
        }
        return handler_(args);
    }

    // 参数验证
    void validateArgs(const CommandArgument& args) const {
        size_t positionalIndex = 0;
        
        for (const auto& def : argDefs_) {
            if (def.isPositional) {
                // 位置参数：检查 positionalArgs_ 是否提供了该位置的值
                if (!def.isOptional && positionalIndex >= args.positionalCount()) {
                    throw exceptions::MissingRequiredArgument(def.name);
                }
                positionalIndex++;
            } else {
                // 命名参数：检查 args_ 中是否存在
                if (!def.isOptional && !args.has(def.name)) {
                    throw exceptions::MissingRequiredArgument(def.name);
                }
            }
        }
    }
};

// ============================================================================
// CommandRegister - 命令注册器
// ============================================================================
class CommandRegister {
private:
    std::shared_ptr<Command> cmd_;
    CommandParser* parser_;
    size_t currentArgIndex_ = 0;

public:
    CommandRegister(std::shared_ptr<Command> cmd, CommandParser* parser = nullptr)
        : cmd_(cmd), parser_(parser) {}

    // 注册参数（位置固定）
    template<typename T>
    CommandRegister& argument(const std::string& name) {
        static_assert(!std::is_same_v<T, bool>, 
                  "bool arguments must be declared using .flag()");
        cmd_->addArgument(name, typeid(T), false, false); // 必填
        currentArgIndex_ = cmd_->getArguments().size() - 1;
        return *this;
    }

    // 注册参数（位置固定，可选）
    template<typename T>
    CommandRegister& argumentOptional(const std::string& name) {
        static_assert(!std::is_same_v<T, bool>, 
                  "bool arguments must be declared using .flag()");
        cmd_->addArgument(name, typeid(T), false, true);
        currentArgIndex_ = cmd_->getArguments().size() - 1;
        return *this;
    }

    // 注册任意位置参数（带标志，如 --message, -i, -o）
    CommandRegister& flag(const std::string& name) {
        cmd_->addArgument(name, typeid(bool), false, true);
        currentArgIndex_ = cmd_->getArguments().size() - 1;
        return *this;
    }

    // 注册位置参数（按顺序）
    template<typename T>
    CommandRegister& positional(const std::string& name) {
        static_assert(!std::is_same_v<T, bool>, 
                  "bool positionals are not supported, use .flag()");
        cmd_->addArgument(name, typeid(T), true, false); // 必填位置参数
        currentArgIndex_ = cmd_->getArguments().size() - 1;
        return *this;
    }

    // 注册位置参数（可选）
    template<typename T>
    CommandRegister& positionalOptional(const std::string& name) {
        static_assert(!std::is_same_v<T, bool>, 
                  "bool positionals are not supported, use .flag()");
        cmd_->addArgument(name, typeid(T), true, true);
        currentArgIndex_ = cmd_->getArguments().size() - 1;
        return *this;
    }

    // 设置执行函数
    CommandRegister& execute(std::function<bool(CommandArgument&)> handler) {
        cmd_->setHandler(handler);
        return *this;
    }

    CommandRegister& alias(const std::string& aliasName) {
        auto& argDefs = cmd_->getArguments2();
        if (currentArgIndex_ >= argDefs.size()) {
            throw exceptions::InvalidCommandSyntax("No argument to add alias to");
        }
        for (const auto& def : argDefs) {
            if (def.name == aliasName) {
                throw exceptions::DuplicateArgument(
                    "Alias name '" + aliasName + "' conflicts with a existing argument"
                );
            }
            if (std::find(def.aliases.begin(), def.aliases.end(), aliasName) != def.aliases.end()) {
                throw exceptions::DuplicateArgumentAlias(
                    aliasName, def.name
                );
            }
        }
        argDefs[currentArgIndex_].addAlias(aliasName);
        return *this;
    }

    CommandRegister& argDescription(const std::string& description) {
        auto& argDefs = cmd_->getArguments2();
        if (currentArgIndex_ >= argDefs.size()) {
            throw exceptions::InvalidCommandSyntax("No argument to set description to");
        }
        argDefs[currentArgIndex_].setDescription(description);
        return *this;
    }

    template<typename T>
    CommandRegister& defaultValue(const T& value) {
        if (currentArgIndex_ >= cmd_->getArguments().size()) {
            throw exceptions::InvalidCommandSyntax(
                "No argument to set default value for"
            );
        }
        
        auto& argDef = cmd_->getArguments2()[currentArgIndex_];
        
        // 类型检查（编译时）
        static_assert(!std::is_same_v<T, bool>, 
                      "Boolean arguments should use .flag()");

        if (!(argDef.isOptional && !argDef.isPositional)) {
            throw exceptions::InvalidCommandSyntax(
                "Only .argumentOptional args can have default values"
            );
        }
        
        // 存储默认值（转为字符串）
        std::stringstream ss;
        ss << value;
        argDef.setDefault(ss.str());
        
        return *this;
    }

    // 设置描述
    CommandRegister& description(const std::string& desc) {
        cmd_->setDescription(desc);
        return *this;
    }

    std::shared_ptr<Command> getCommand() const { return cmd_; }
};

// ============================================================================
// CommandParser - 核心解析器
// ============================================================================
class CommandParser {
private:
    std::unordered_map<std::string, std::shared_ptr<Command>> commands_;
    std::unordered_map<std::string, std::string> aliasToCommand_;
    std::string programName_;
    bool caseSensitive_ = true;

    // 内部解析函数
    std::pair<std::string, CommandArgument> parseTokens(const std::vector<std::string>& tokens) const {
        if (tokens.empty()) {
            throw exceptions::InvalidCommandSyntax("Empty command");
        }

        std::string cmdName = tokens[0];
        if (!caseSensitive_) { // 不区分大小写
            // 转小写
            std::transform(cmdName.begin(), cmdName.end(), cmdName.begin(), ::tolower);
        }

        // 查找命令
        auto it = commands_.find(cmdName);
        if (it == commands_.end()) {
            // 尝试别名
            auto aliasIt = aliasToCommand_.find(cmdName);
            if (aliasIt != aliasToCommand_.end()) {
                cmdName = aliasIt->second;
                it = commands_.find(cmdName);
            }
        }

        if (it == commands_.end()) {
            throw exceptions::UnknownCommand(tokens[0]);
        }

        const auto& cmd = it->second;
        CommandArgument args;

        // 解析参数
        std::string currentArgName;
        bool expectValue = false;
        bool stopParsing = false;
        std::vector<std::string> queuePositionals;

        auto findArgDef = [&](const std::string& name) -> const Command::ArgumentDef* {
            CMDPARSER_DEBUG_PRINT("Finding Argument: %s\n", name.c_str());
            for (const auto& argDef : cmd->getArguments()) {
                if (argDef.name == name ||
                    std::find(argDef.aliases.begin(), argDef.aliases.end(), name) != argDef.aliases.end()) {
                    return &argDef;
                }
            }
            return nullptr;
        };

        for (size_t i = 1; i < tokens.size(); i++) {
            const std::string& token = tokens[i];
            if (stopParsing) {
                CMDPARSER_DEBUG_PRINT("Pushing Token to positional queue: %s\n", token.c_str());
                queuePositionals.push_back(token);
                continue;
            } else if (token == "--") {
                stopParsing = true;
                continue;
            }
            CMDPARSER_DEBUG_PRINT("Processing Token: %s\n", token.c_str());

            if (expectValue) {
                if (token.empty()) {
                    throw exceptions::InvalidCommandSyntax("Empty value for argument: " + currentArgName);
                }

                const auto* argDef = findArgDef(currentArgName);
                if (argDef) {
                    args.set(currentArgName, ArgumentValue(token, argDef->type));
                } else {
                    args.set(currentArgName, ArgumentValue(token));
                }

                expectValue = false;
                currentArgName.clear();
                continue;
            }

            // 检查是否是标志参数（以 - 或 -- 开头）
            if (token.size() > 1 && (token[0] == '-' || token[0] == '/')) {
                const auto* argDef = findArgDef(token);

                if (argDef != nullptr) {
                    if (argDef->type == typeid(bool)) {
                        args.set(argDef->name, true);
                    } else if (i + 1 < tokens.size() && !tokens[i + 1].empty()) {
                        args.set(argDef->name, ArgumentValue(tokens[i + 1], argDef->type));
                        i++; // 跳过值
                    } else if (!argDef->isOptional) {
                        throw exceptions::MissingRequiredArgument(argDef->name);
                    }
                } else if (token[0] == '-' && token.size() > 2 && token[1] != '-') {
                    // 短选项展开：-abc 变成 -a -b -c，但如果某个短选项需要值，
                    // 则优先把后缀内容当成该选项的值（如 -uamy）
                    bool handled = false;
                    for (size_t j = 1; j < token.size(); j++) {
                        std::string flagName = std::string("-") + token[j];
                        CMDPARSER_DEBUG_PRINT("Expanded Flag: %s\n", flagName.c_str());
                        const auto* shortArgDef = findArgDef(flagName);
                        if (shortArgDef == nullptr) {
                            CMDPARSER_DEBUG_PRINT("Unknown Flag: %s\n", flagName.c_str());
                            CMDPARSER_DEBUG_PRINT("Pushing Token to positional queue: %s\n", token.c_str());
                            queuePositionals.push_back(token);
                            handled = true;
                            break;
                        }

                        if (shortArgDef->type == typeid(bool)) {
                            args.set(shortArgDef->name, true);
                            continue;
                        }

                        if (j + 1 < token.size()) {
                            const std::string value = token.substr(j + 1);
                            args.set(shortArgDef->name, ArgumentValue(value, shortArgDef->type));
                            handled = true;
                            break;
                        }

                        if (i + 1 < tokens.size() && !tokens[i + 1].empty()) {
                            if (shortArgDef->type == typeid(int)) {
                                args.set(shortArgDef->name, std::stoi(tokens[i + 1]));
                            } else {
                                args.set(shortArgDef->name, ArgumentValue(tokens[i + 1]));
                            }
                            i++;
                        } else if (!shortArgDef->isOptional) {
                            throw exceptions::MissingRequiredArgument(shortArgDef->name);
                        }
                        handled = true;
                        break;
                    }
                    if (!handled) {
                        handled = true;
                    }
                    if (!handled) {
                        CMDPARSER_DEBUG_PRINT("Pushing Token to positional queue: %s\n", token.c_str());
                        queuePositionals.push_back(token);
                    }
                } else {
                    // 未知标志，作为位置参数
                    CMDPARSER_DEBUG_PRINT("Pushing Token to positional queue: %s\n", token.c_str());
                    queuePositionals.push_back(token);
                }
            } else {
                // 位置参数
                CMDPARSER_DEBUG_PRINT("Pushing Token to positional queue: %s\n", token.c_str());
                queuePositionals.push_back(token);
            }
        }
        // ============================================================================
        // Add Positionals + Fill Defaults
        // ============================================================================

        size_t positionalIndex = 0;
        bool hasVariadic = false;
        size_t variadicIdx = (size_t)-1;

        // 1. 查找变长参数
        for (size_t i = 0; i < cmd->getArguments().size(); i++) {
            const auto& def = cmd->getArguments()[i];
            if (def.isPositional && __internal::isVariadicArg(def.name)) {
                hasVariadic = true;
                variadicIdx = i;
                break;
            }
        }

        // 2. 分配位置参数
        for (size_t i = 0; i < cmd->getArguments().size(); i++) {
            const auto& def = cmd->getArguments()[i];
            
            if (!def.isPositional) {
                continue;  // 命名参数稍后处理
            }
            
            // 变长参数：吃掉所有剩余 token
            if (hasVariadic && i == variadicIdx) {
                while (positionalIndex < queuePositionals.size()) {
                    args.addPositional(ArgumentValue(
                        queuePositionals[positionalIndex++], 
                        def.type
                    ));
                }
                continue;
            }
            
            // 普通位置参数
            if (positionalIndex < queuePositionals.size()) {
                args.addPositional(ArgumentValue(
                    queuePositionals[positionalIndex++], 
                    def.type
                ));
            } else if (!def.isOptional) {
                throw exceptions::MissingRequiredArgument(def.name);
            }
        }

        // 3. 检查多余的 token（没有变长参数时）
        if (!hasVariadic && positionalIndex < queuePositionals.size()) {
            CMDPARSER_DEBUG_PRINT("positionalIndex: %d, queuePositionals size: %d\n", positionalIndex, queuePositionals.size());
            std::string rawData;
            for (const auto& data : queuePositionals) {
                rawData += data;
                rawData += ",";
            }
            CMDPARSER_DEBUG_PRINT("Data: %s\n", rawData.c_str());
            throw exceptions::InvalidCommandSyntax(
                "Too many positional arguments"
            );
        }

        // 4. 填充默认值（对所有命名参数）
        for (const auto& def : cmd->getArguments()) {
            if (!def.isPositional && def.hasDefault && !args.has(def.name)) {
                args.set(def.name, ArgumentValue(def.defaultValue, def.type));
            }
        }
        // 验证必填参数
        cmd->validateArgs(args);

        return {cmdName, std::move(args)};
    }

    bool executeCommand(const std::string& name, CommandArgument& args) {
        auto it = commands_.find(name);
        if (it == commands_.end()) {
            throw exceptions::UnknownCommand(name);
        }
        const auto& cmd = it->second;
        return cmd->execute(args);
    }

public:
    CommandParser(const std::string& programName = "program") 
        : programName_(programName) {}

    // 注册命令（返回注册器）
    CommandRegister registerCommand(const std::string& name) {
        if (commands_.find(name) != commands_.end()) {
            throw exceptions::DuplicateCommand(name);
        }
        auto cmd = std::make_shared<Command>(name);
        commands_[name] = cmd;
        return CommandRegister(cmd, this);
    }

    // 注册别名
    void registerAlias(const std::string& alias, const std::string& command) {
        if (commands_.find(command) == commands_.end()) {
            throw exceptions::UnknownCommand(command);
        }
        aliasToCommand_[alias] = command;
    }

    // 解析命令行字符串
    bool parse(const std::string& cmdLine) {
        // 简单的分词（支持引号）
        std::vector<std::string> tokens;
        std::string current;
        bool inQuotes = false;
        char quoteChar = '"';

        for (char c : cmdLine) {
            if (inQuotes) {
                if (c == quoteChar && !current.empty()) {
                    tokens.push_back(current); // push token
                    current.clear(); // clear current
                    inQuotes = false; // 取消设置状态
                } else {
                    current += c; // 按原样处理
                }
            } else if (c == '"' || c == '\'') { // 是quote
                inQuotes = true;
                quoteChar = c;
            } else if (std::isspace(c) && !current.empty()) { // 遇到空格
                tokens.push_back(current); // push token
                current.clear(); // clear current
            } else {
                current += c;
            }
        }
        if (inQuotes) {
            throw exceptions::InvalidCommandSyntax("Unclosed Quote");
        }

        if (!current.empty()) {
            tokens.push_back(current);
        }

        if (tokens.empty()) {
            throw exceptions::InvalidCommandSyntax("Empty command line");
        }

        auto result = parseTokens(tokens);
        return executeCommand(result.first, result.second);
    }

    // 解析 argv/argc（标准main参数）
    bool parse(int argc, char* argv[]) {
        std::vector<std::string> tokens;
        // 跳过程序名（argv[0]）
        for (int i = 1; i < argc; i++) {
            tokens.push_back(argv[i]);
        }
        if (tokens.empty()) {
            throw exceptions::InvalidCommandSyntax("No command specified");
        }
        // 然后可以直接用（因为argv已经帮你切好了）
        auto result = parseTokens(tokens);
        return executeCommand(result.first, result.second);
    }

    // ============================================================================
    // Help Generation
    // ============================================================================

    std::string getHelp() const {
        std::stringstream ss;
        ss << "Usage: " << programName_ << " <command> [options]\n\n";
        ss << "Commands:\n";
        
        for (const auto& pair : commands_) {
            const auto& cmd = pair.second;
            ss << "  " << cmd->getName();
            
            if (!cmd->getDescription().empty()) {
                ss << "  - " << cmd->getDescription();
            }
            ss << "\n";
        }
        
        ss << "\n";
        ss << "Global Options:\n";
        ss << "  --help, -h  Show this help message\n";
        ss << "  --version   Show version information\n";
        ss << "\n";
        ss << "For more help on a specific command:\n";
        ss << "  " << programName_ << " help <command>\n";
        
        return ss.str();
    }

    std::string getHelp(const std::string& commandName) const {
        auto it = commands_.find(commandName);
        if (it == commands_.end()) {
            return "Unknown command: " + commandName;
        }
        
        const auto& cmd = it->second;
        std::stringstream ss;
        
        // 命令名称和描述
        ss << "Command: " << cmd->getName() << "\n";
        if (!cmd->getDescription().empty()) {
            ss << cmd->getDescription() << "\n";
        }
        ss << "\n";
        
        // 用法
        ss << "Usage: " << programName_ << " " << cmd->getName();
        
        // 添加命名参数
        bool hasNamedArgs = false;
        for (const auto& arg : cmd->getArguments()) {
            if (!arg.isPositional) {
                if (!hasNamedArgs) {
                    ss << " [options]";
                    hasNamedArgs = true;
                }
            }
        }
        
        // 添加位置参数
        for (const auto& arg : cmd->getArguments()) {
            if (arg.isPositional) {
                if (__internal::isVariadicArg(arg.name)) {
                    if (arg.isOptional) {
                        ss << " [" << arg.name << "]";
                    } else {
                        ss << " " << arg.name;
                    }
                } else {
                    if (arg.isOptional) {
                        ss << " [" << arg.name << "]";
                    } else {
                        ss << " " << arg.name;
                    }
                }
            }
        }
        ss << "\n\n";
        
        // 命名参数（选项）
        bool hasOptions = false;
        for (const auto& arg : cmd->getArguments()) {
            if (!arg.isPositional) {
                if (!hasOptions) {
                    ss << "Options:\n";
                    hasOptions = true;
                }
                
                // 选项名称（包括别名）
                std::string names = arg.name;
                for (const auto& alias : arg.aliases) {
                    names += ", " + alias;
                }
                ss << "  " << names;
                
                // 类型
                ss << " <" << demangle(arg.type.name()) << ">";
                
                // 是否必填
                if (arg.isOptional) {
                    ss << " (optional)";
                } else {
                    ss << " (required)";
                }
                
                // 默认值
                if (arg.isOptional && arg.hasDefault) {
                    ss << " [default: " << arg.defaultValue << "]";
                }
                
                // 描述
                if (!arg.description.empty()) {
                    ss << "\n      " << arg.description;
                }
                
                ss << "\n";
            }
        }
        
        if (hasOptions) ss << "\n";
        
        // 位置参数
        bool hasPositionals = false;
        for (const auto& arg : cmd->getArguments()) {
            if (arg.isPositional) {
                if (!hasPositionals) {
                    ss << "Arguments:\n";
                    hasPositionals = true;
                }
                
                ss << "  " << arg.name;
                
                // 变长参数标识
                if (__internal::isVariadicArg(arg.name)) {
                    ss << " (variadic)";
                    if (arg.isOptional) {
                        ss << " [0 or more]";
                    } else {
                        ss << " [1 or more]";
                    }
                } else {
                    if (arg.isOptional) {
                        ss << " (optional)";
                    } else {
                        ss << " (required)";
                    }
                }
                
                // 类型
                ss << " <" << demangle(arg.type.name()) << ">";
                
                // 默认值（只有可选且有默认值时）
                if (arg.isOptional && arg.hasDefault) {
                    ss << " [default: " << arg.defaultValue << "]";
                }
                
                // 描述
                if (!arg.description.empty()) {
                    ss << "\n      " << arg.description;
                }
                
                ss << "\n";
            }
        }
        
        if (hasPositionals) ss << "\n";
        
        // 执行器提示
        ss << "Examples:\n";
        ss << "  " << programName_ << " " << cmd->getName();
        
        // 找一个示例
        bool first = true;
        for (const auto& arg : cmd->getArguments()) {
            if (arg.isPositional) {
                if (__internal::isVariadicArg(arg.name)) {
                    if (arg.isOptional) {
                        ss << " [value...]";
                    } else {
                        ss << " value...";
                    }
                } else {
                    if (arg.isOptional) {
                        ss << " [" << arg.name << "]";
                    } else {
                        ss << " " << arg.name;
                    }
                }
            } else {
                if (first && !arg.isOptional) {
                    ss << " " << arg.name << " value";
                    first = false;
                }
            }
        }
        ss << "\n";
        
        return ss.str();
    }

    // 辅助函数：demangle 类型名（可选）
    std::string demangle(const std::string& mangled) const {
        if (mangled == "i") return "int";
        if (mangled == "l") return "long";
        if (mangled == "x") return "long long";
        if (mangled == "j") return "unsigned int";
        if (mangled == "m") return "unsigned long";
        if (mangled == "y") return "unsigned long long";
        if (mangled == "f") return "float";
        if (mangled == "d") return "double";
        if (mangled == "e") return "long double";
        if (mangled == "b") return "bool";
        if (mangled == "NSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE") return "string";
        return mangled;
    }

    // 设置大小写敏感
    void setCaseSensitive(bool sensitive) { caseSensitive_ = sensitive; }

    // 获取所有命令名
    std::vector<std::string> getCommandNames() const {
        std::vector<std::string> names;
        for (const auto& pair : commands_) {
            names.push_back(pair.first);
        }
        return names;
    }
};
} // namespace cmdparser
