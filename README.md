# CmdParser

A lightweight C++ CLI command parser library supporting subcommands, positional arguments, flag arguments, and type-safe parameter retrieval.

## Features

- **Subcommand support** - Register nested commands like `git commit`, `git push`
- **Multiple argument types** - Positional arguments, optional arguments, and boolean flags
- **Type-safe retrieval** - Get arguments with automatic type conversion
- **Alias support** - Create command aliases (e.g., `ci` → `commit`)
- **Help generation** - Built-in help text support
- **Exception handling** - Clear error messages for syntax errors

## Quick Start

### Basic Usage

```cpp
#include "cmdparser.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    using namespace cmdparser;

    CommandParser parser("mycli");

    // Register a command
    parser.registerCommand("greet")
        .description("Greet someone")
        .argument<std::string>("-n")  // Named argument
        .execute([](CommandArgument& args) -> bool {
            std::string name = args.get<std::string>("-n");
            std::cout << "Hello, " << name << "!" << std::endl;
            return true;
        });

    try {
        return parser.parse(argc, argv) ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

### Compile and Run

```bash
g++ -std=c++17 -o mycli main.cpp
./mycli greet -n World
# Output: Hello, World!
```

## API Reference

### CommandParser

```cpp
CommandParser parser("program_name");
```

| Method | Description |
|--------|-------------|
| `registerCommand(name)` | Register a new command, returns `CommandRegister` |
| `registerAlias(alias, command)` | Create alias for existing command |
| `parse(argc, argv)` | Parse standard main() arguments |
| `parse(cmdLine)` | Parse command line string |

### CommandRegister

Chainable builder for command configuration:

```cpp
parser.registerCommand("cmd")
    .description("Description text")
    .argument<T>("name")           // Required named argument
    .argumentOptional<T>("name")   // Optional named argument
    .argumentFlag<T>("name")       // Flag argument (boolean)
    .positional<T>("name")          // Required positional argument
    .positionalOptional<T>("name")  // Optional positional argument
    .execute(handler);
```

### CommandArgument

Access parsed arguments in your handler:

```cpp
execute([](CommandArgument& args) -> bool {
    // Named arguments
    std::string value = args.get<std::string>("arg_name");
    bool flag = args.has("flag_name");
    
    // Positional arguments
    std::string pos = args.getPositional(0);
    size_t count = args.positionalCount();
    
    // Check existence
    if (args.has("optional_arg")) {
        // ...
    }
    
    return true; // Return bool to show command execution result
});
```

## Examples

### Example 1: Git-like Commit Command

```cpp
parser.registerCommand("commit")
    .description("Commit changes")
    .argument<std::string>("-m")           // Required message
    .argumentFlag<bool>("-a")              // Optional all flag
    .positional<std::string>("file")        // Required file
    .execute([](CommandArgument& args) -> bool {
        std::string message = args.get<std::string>("-m");
        bool all = args.has("-a");
        std::string file = args.getPositional(0);
        
        std::cout << "Committing: " << file << std::endl;
        std::cout << "Message: " << message << std::endl;
        std::cout << "All: " << (all ? "yes" : "no") << std::endl;
        return true;
    });
```

Usage:
```bash
mycli commit -m "Initial commit" README.md
mycli commit -a -m "Update all" .
```

### Example 2: Push Command with Optional Arguments

```cpp
parser.registerCommand("push")
    .description("Push to remote")
    .argumentFlag<bool>("--force")
    .argumentOptional<std::string>("--remote")
    .positional<std::string>("branch")
    .execute([](CommandArgument& args) -> bool {
        std::string branch = args.getPositional(0);
        bool force = args.has("--force");
        std::string remote = args.has("--remote") 
            ? args.get<std::string>("--remote") 
            : "origin";

        std::cout << "Pushing to " << remote << "/" << branch;
        if (force) std::cout << " (force)";
        std::cout << std::endl;
        return true;
    });
```

Usage:
```bash
mycli push main
mycli push --remote origin --force feature-branch
```

### Example 3: Command Aliases

```cpp
parser.registerCommand("commit")
    .description("Commit changes")
    // ... arguments ...

parser.registerAlias("ci", "commit");  // 'ci' is now alias for 'commit'
```

Usage:
```bash
mycli ci -m "Quick commit" file.txt
```

### Example 4: Parse Command Line String

```cpp
// Useful for embedded shells or REPLs
bool result = parser.parse("commit -m \"Fix bug\" main.cpp");
```

## Argument Types

The library uses type erasure internally and supports these common types:

| Type | Usage | Description |
|------|-------|-------------|
| `std::string` | `.argument<std::string>("-n")` | String values |
| `int` | `.argument<int>("--port")` | Integer values |
| `bool` | `.argumentFlag<bool>("-v")` | Boolean flags |

Type conversion is performed when calling `args.get<T>()`.

## Exception Handling

```cpp
try {
    parser.parse(argc, argv);
} catch (const exceptions::InvalidCommandSyntax& e) {
    // Syntax errors (missing args, invalid format)
} catch (const exceptions::UnknownCommand& e) {
    // Command not found
} catch (const exceptions::MissingRequiredArgument& e) {
    // Required argument missing
} catch (const std::exception& e) {
    // Other errors
}
```

## Building

### Requirements
- C++17 or later
- Standard library only (no external dependencies)

### Compilation

```bash
# Simple compilation
g++ -std=c++17 -o myapp main.cpp

# With optimizations
g++ -std=c++17 -O2 -o myapp main.cpp
```

## Architecture

```
CommandParser
    └── Command (registered commands)
            ├── ArgumentDef (parameter definitions)
            └── Handler (executed function)
    └── CommandArgument (parsed values)
            ├── Named args (unordered_map)
            └── Positional args (vector)
```

## License

This project is licensed under the MIT License.
See the [LICENSE](LICENSE) file for details.