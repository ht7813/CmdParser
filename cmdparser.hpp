// CLI Command Parser
// by ht7813
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

namespace cmdparser {

// 前置声明
class CommandArgument;
class Command;
class CommandRegister;
class CommandParser;

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
// 类型擦除的值存储（用于支持任意类型参数）
// ============================================================================
class ArgumentValue {
private:
    std::type_index type_;
    void* data_;
    std::function<void(void*)> deleter_;
    std::function<std::string(const void*)> toString_;

public:
    // 默认构造（空值）
    ArgumentValue() : type_(typeid(void)), data_(nullptr), deleter_(nullptr), toString_(nullptr) {}

    template<typename T>
    explicit ArgumentValue(const T& value)
        : type_(typeid(T))
        , data_(new T(value))
        , deleter_([](void* p) { delete static_cast<T*>(p); })
        , toString_([](const void* p) { 
            std::stringstream ss;
            ss << *static_cast<const T*>(p);
            return ss.str();
        }) {}

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
        }
        return *this;
    }

    template<typename T>
    const T& as() const {
        if (type_ != typeid(T)) {
            throw exceptions::ArgumentTypeMismatch("Cannot cast argument to requested type");
        }
        return *static_cast<const T*>(data_);
    }

    std::string toString() const {
        if (toString_ && data_) {
            return toString_(data_);
        }
        return "";
    }

    std::type_index type() const { return type_; }
    bool isEmpty() const { return data_ == nullptr; }
};

// ============================================================================
// CommandArgument - 解析后的参数集合
// ============================================================================
class CommandArgument {
private:
    std::unordered_map<std::string, ArgumentValue> args_;
    std::vector<std::string> positionalArgs_; // 位置参数

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


    void addPositional(const std::string& value) {
        positionalArgs_.push_back(value);
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
    const std::string& getPositional(size_t index) const {
        if (index >= positionalArgs_.size()) {
            throw exceptions::InvalidCommandSyntax("Positional argument index out of range");
        }
        return positionalArgs_[index];
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
            printf("  [%zu] = %s\n", i, positionalArgs_[i].c_str());
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
        std::string defaultValue;
        std::string description;
        std::vector<std::string> aliases; // 可选：短选项别名

        ArgumentDef(const std::string& n, const std::type_info& t, bool pos = false, bool opt = true)
            : name(n), type(t), isPositional(pos), isOptional(opt) {}
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
        argIndex_[name] = argDefs_.size();
        argDefs_.emplace_back(name, type, positional, optional);
    }

    void setHandler(std::function<bool(CommandArgument&)> handler) {
        handler_ = handler;
        hasHandler_ = true;
    }

    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
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

public:
    CommandRegister(std::shared_ptr<Command> cmd, CommandParser* parser = nullptr)
        : cmd_(cmd), parser_(parser) {}

    // 注册子命令
    CommandRegister subcmd(const std::string& name);

    // 注册参数（位置固定）
    template<typename T>
    CommandRegister& argument(const std::string& name) {
        cmd_->addArgument(name, typeid(T), false, false); // 必填
        return *this;
    }

    // 注册参数（位置固定，可选）
    template<typename T>
    CommandRegister& argumentOptional(const std::string& name) {
        cmd_->addArgument(name, typeid(T), false, true);
        return *this;
    }

    // 注册任意位置参数（带标志，如 --message, -i, -o）
    template<typename T>
    CommandRegister& argumentFlag(const std::string& name) {
        cmd_->addArgument(name, typeid(T), false, true);
        return *this;
    }

    // 注册位置参数（按顺序）
    template<typename T>
    CommandRegister& positional(const std::string& name) {
        cmd_->addArgument(name, typeid(T), true, false); // 必填位置参数
        return *this;
    }

    // 注册位置参数（可选）
    template<typename T>
    CommandRegister& positionalOptional(const std::string& name) {
        cmd_->addArgument(name, typeid(T), true, true);
        return *this;
    }

    // 设置执行函数
    CommandRegister& execute(std::function<bool(CommandArgument&)> handler) {
        cmd_->setHandler(handler);
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
        if (!caseSensitive_) {
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

        for (size_t i = 1; i < tokens.size(); i++) {
            const std::string& token = tokens[i];

            if (expectValue) {
                // 读取参数值
                if (token.empty()) {
                    throw exceptions::InvalidCommandSyntax("Empty value for argument: " + currentArgName);
                }
                // 根据类型存储值（简化：都存为字符串，类型转换在get时进行）
                args.set(currentArgName, ArgumentValue(token));
                expectValue = false;
                currentArgName.clear();
                continue;
            }

            // 检查是否是标志参数（以 - 或 -- 开头）
            if (token.size() > 1 && (token[0] == '-' || token[0] == '/')) {
                // 检查是否所有字符都是字母（短选项如 -abc 可能展开）
                if (token[0] == '-' && token.size() > 2 && token[1] != '-') {
                    // 短选项展开：-abc 变成 -a -b -c
                    for (size_t j = 1; j < token.size(); j++) {
                        std::string flagName = std::string("-") + token[j];
                        // 查找是否存在该标志
                        bool found = false;
                        for (const auto& argDef : cmd->getArguments()) {
                            if (argDef.name == flagName || 
                                std::find(argDef.aliases.begin(), argDef.aliases.end(), flagName) != argDef.aliases.end()) {
                                // 处理标志
                                args.set(argDef.name, ArgumentValue(true));
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            // 当作位置参数
                            args.addPositional(token);
                        }
                    }
                    continue;
                }

                // 查找参数定义
                bool found = false;
                for (const auto& argDef : cmd->getArguments()) {
                    if (argDef.name == token || 
                        std::find(argDef.aliases.begin(), argDef.aliases.end(), token) != argDef.aliases.end()) {
                        // 检查下一个token是否是值
                        if (i + 1 < tokens.size() && tokens[i + 1][0] != '-') {
                            args.set(argDef.name, ArgumentValue(tokens[i + 1]));
                            i++; // 跳过值
                        } else {
                            // 布尔标志
                            args.set(argDef.name, ArgumentValue(true));
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    // 未知标志，作为位置参数
                    args.addPositional(token);
                }
            } else {
                // 位置参数
                args.addPositional(token);
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
            if (c == '"' || c == '\'') {
                if (!inQuotes) {
                    inQuotes = true;
                    quoteChar = c;
                } else if (c == quoteChar) {
                    inQuotes = false;
                } else {
                    current += c;
                }
            } else if (std::isspace(c) && !inQuotes) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
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
        auto result = parseTokens(tokens);
        return executeCommand(result.first, result.second);
    }

    // 获取帮助信息
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
        return ss.str();
    }

    // 获取特定命令的帮助
    std::string getHelp(const std::string& commandName) const {
        auto it = commands_.find(commandName);
        if (it == commands_.end()) {
            return "Unknown command: " + commandName;
        }
        const auto& cmd = it->second;
        std::stringstream ss;
        ss << "Command: " << cmd->getName() << "\n";
        if (!cmd->getDescription().empty()) {
            ss << "  " << cmd->getDescription() << "\n";
        }
        ss << "Arguments:\n";
        for (const auto& arg : cmd->getArguments()) {
            ss << "  " << arg.name;
            if (arg.isOptional) ss << " (optional)";
            else ss << " (required)";
            if (arg.isPositional) ss << " [positional]";
            ss << " : " << arg.type.name() << "\n";
        }
        return ss.str();
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

// ============================================================================
// CommandRegister 的实现（需要 CommandParser 完整定义）
// ============================================================================
inline CommandRegister CommandRegister::subcmd(const std::string& name) {
    if (parser_) {
        return parser_->registerCommand(name);
    }
    // 如果没有 parser，返回一个空的注册器
    auto cmd = std::make_shared<Command>(name);
    return CommandRegister(cmd, nullptr);
}

} // namespace cmdparser