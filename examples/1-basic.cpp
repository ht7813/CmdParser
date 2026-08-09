// ============================================================================
// Example 1: Basic Usage
// ============================================================================
// Demonstrates:
// - Command Register
// - flag、argument、positional
// - Argument Get
// ============================================================================

#include "cmdparser.hpp"
#include <iostream>
#include <string>

using namespace cmdparser;

int main(int argc, char* argv[]) {
    CommandParser parser("example1");
    
    parser.registerCommand("greet")
        .description("Greet someone with a message")
        .flag("-v")
            .alias("--verbose")
            .argDescription("Be verbose")
        .argument<std::string>("-n")
            .alias("--name")
            .argDescription("Name of the person to greet")
        .argumentOptional<std::string>("-m")
            .alias("--message")
            .defaultValue("Hello")
            .argDescription("Greeting message")
        .positionalOptional<std::string>("times")
            .argDescription("Number of times to greet (default: 1)")
        .execute([](CommandArgument& args) -> bool {
            std::string name = args.get<std::string>("-n");
            std::string msg = args.get<std::string>("-m");
            bool verbose = args.has("-v");
            int times = 1;
            
            if (args.positionalCount() > 0) {
                times = args.getPositional<int>(0);
            }
            
            for (int i = 0; i < times; i++) {
                if (verbose) {
                    std::cout << "[" << (i+1) << "/" << times << "] ";
                }
                std::cout << msg << ", " << name << "!" << std::endl;
            }
            return true;
        });
    
    parser.registerCommand("help")
        .description("Show help information")
        .execute([&parser](CommandArgument& args) -> bool {
            std::cout << parser.getHelp() << std::endl;
            return true;
        });
    
    try {
        return parser.parse(argc, argv) ? 0 : 1;
    } catch (const exceptions::InvalidCommandSyntax& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << parser.getHelp() << std::endl;
        return 1;
    }
}