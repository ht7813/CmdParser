// ============================================================================
// Example 5: Shell/REPL Style
// ============================================================================
// Demonstrates:
// - parse(cmdLine) for interactive shell
// - command line parse
// - Quote
// ============================================================================

#include "cmdparser.hpp"
#include <iostream>
#include <string>

using namespace cmdparser;

int main() {
    CommandParser parser("shell");
    
    parser.registerCommand("echo")
        .description("Echo text")
        .positionalOptional<std::string>("text")
            .argDescription("Text to echo")
        .execute([](CommandArgument& args) -> bool {
            std::cout << ((args.positionalCount() > 0) ? args.getPositional<std::string>(0) : "") << std::endl;
            return true;
        });
    
    parser.registerCommand("calc")
        .description("Simple calculator")
        .positional<int>("a")
            .argDescription("First number")
        .positional<std::string>("op")
            .argDescription("Operation (+, -, *, /)")
        .positional<int>("b")
            .argDescription("Second number")
        .execute([](CommandArgument& args) -> bool {
            int a = args.getPositional<int>(0);
            std::string op = args.getPositional<std::string>(1);
            int b = args.getPositional<int>(2);
            
            if (op == "+") std::cout << a + b << std::endl;
            else if (op == "-") std::cout << a - b << std::endl;
            else if (op == "*") std::cout << a * b << std::endl;
            else if (op == "/") std::cout << a / b << std::endl;
            else {
                std::cerr << "Unknown operation: " << op << std::endl;
                return false;
            }
            return true;
        });
    
    parser.registerCommand("exit")
        .description("Exit shell")
        .execute([](CommandArgument& args) -> bool {
            std::cout << "Goodbye!" << std::endl;
            exit(0);
            return true;
        });
    
    parser.registerCommand("help")
        .description("Show help information")
        .positionalOptional<std::string>("command")
            .argDescription("Command to show help for")
        .execute([&parser](CommandArgument& args) -> bool {
            if (args.positionalCount() == 1) {
                std::string cmd = args.getPositional<std::string>(0);
                std::cout << parser.getHelp(cmd) << std::endl;
            } else {
                std::cout << parser.getHelp() << std::endl;
            }
            return true;
        });
    
    std::cout << "CmdParser Shell v2" << std::endl;
    std::cout << "Type 'help' for commands, 'exit' to quit" << std::endl;
    std::cout << std::endl;
    
    std::string line;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);
        
        if (line.empty()) continue;
        
        try {
            parser.parse(line);
        } catch (const exceptions::InvalidCommandSyntax& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
    
    return 0;
}