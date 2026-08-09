// ============================================================================
// Example 3: Package Manager CLI
// ============================================================================
// Demonstrates:
// - Variadic Positional Arguments
// - Default Values
// - Argument Aliases
// - Optional and Required Arguments
// ============================================================================

#include "cmdparser.hpp"
#include <iostream>
#include <vector>
#include <string>

using namespace cmdparser;

int main(int argc, char* argv[]) {
    CommandParser parser("pkg");
    
    // ==================== install command ====================
    parser.registerCommand("install")
        .description("Install packages")
        .flag("-y")
            .alias("--yes")
            .argDescription("Assume yes to all prompts")
        .flag("--no-deps")
            .argDescription("Don't install dependencies")
        .argumentOptional<std::string>("--prefix")
            .defaultValue("/usr/local")
            .alias("-p")
            .argDescription("Installation prefix")
        .argumentOptional<std::string>("--version")
            .alias("-v")
            .argDescription("Version constraint")
        .positional<std::string>("packages...")
            .argDescription("Packages to install (at least one)")
        .execute([](CommandArgument& args) -> bool {
            bool yes = args.has("-y");
            bool noDeps = args.has("--no-deps");
            std::string prefix = args.get<std::string>("--prefix");
            
            std::cout << "Installing packages:" << std::endl;
            for (size_t i = 0; i < args.positionalCount(); i++) {
                std::cout << "  - " << args.getPositional<std::string>(i) << std::endl;
            }
            std::cout << "  Prefix: " << prefix << std::endl;
            std::cout << "  Confirm: " << (yes ? "yes (auto)" : "ask") << std::endl;
            std::cout << "  Dependencies: " << (noDeps ? "no" : "yes") << std::endl;
            return true;
        });
    
    // ==================== remove command ====================
    parser.registerCommand("remove")
        .description("Remove packages")
        .flag("--purge")
            .alias("-p")
            .argDescription("Remove configuration files too")
        .flag("--recursive")
            .alias("-r")
            .argDescription("Remove dependencies recursively")
        .positional<std::string>("packages...")
            .argDescription("Packages to remove (at least one)")
        .execute([](CommandArgument& args) -> bool {
            bool purge = args.has("--purge");
            bool recursive = false;
            if (args.has("--recursive")) {
                recursive = args.has("--recursive");
            }
            
            std::cout << "Removing packages:" << std::endl;
            for (size_t i = 0; i < args.positionalCount(); i++) {
                std::cout << "  - " << args.getPositional<std::string>(i) << std::endl;
            }
            std::cout << "  Purge: " << (purge ? "yes" : "no") << std::endl;
            std::cout << "  Recursive: " << (recursive ? "yes" : "no") << std::endl;
            return true;
        });
    
    // ==================== search command ====================
    parser.registerCommand("search")
        .description("Search for packages")
        .positional<std::string>("query")
            .argDescription("Search query")
        .argumentOptional<int>("--limit")
            .defaultValue(10)
            .alias("-l")
            .argDescription("Maximum number of results")
        .argumentOptional<std::string>("--source")
            .defaultValue("all")
            .alias("-s")
            .argDescription("Repository source")
        .execute([](CommandArgument& args) -> bool {
            std::string query = args.getPositional<std::string>(0);
            int limit = args.get<int>("--limit");
            std::string source = args.get<std::string>("--source");
            
            std::cout << "Searching for: " << query << std::endl;
            std::cout << "  Limit: " << limit << std::endl;
            std::cout << "  Source: " << source << std::endl;
            
            // Simulate search results
            for (int i = 0; i < limit && i < 5; i++) {
                std::cout << "    - package-" << i << " (matching " << query << ")" << std::endl;
            }
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
    
    try {
        return parser.parse(argc, argv) ? 0 : 1;
    } catch (const exceptions::InvalidCommandSyntax& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Try: ./pkg help" << std::endl;
        return 1;
    }
}
