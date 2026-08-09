// ============================================================================
// Example 2: Git-like CLI
// ============================================================================
// Demonstrates:
// - Multi-Commands
// - Command Aliases
// - Variadic Positional Arguments (files...)
// - -- Stop Parse
// ============================================================================

#include "cmdparser.hpp"
#include <iostream>
#include <vector>
#include <string>

using namespace cmdparser;

int main(int argc, char* argv[]) {
    CommandParser parser("mygit");
    
    // ==================== commit command ====================
    parser.registerCommand("commit")
        .description("Record changes to the repository")
        .flag("-a")
            .alias("--all")
            .argDescription("Automatically stage all tracked files")
        .flag("-m")
            .alias("--message")
            .argDescription("Commit message (can be used multiple times)")
        .argument<std::string>("-m")
            .alias("--msg")
            .argDescription("Commit message")
        .argumentOptional<std::string>("--author")
            .defaultValue("unknown")
            .argDescription("Author of the commit")
        .positionalOptional<std::string>("files...")
            .argDescription("Files to commit")
        .execute([](CommandArgument& args) -> bool {
            bool all = args.has("-a");
            std::string msg = args.get<std::string>("-m");
            std::string author = args.get<std::string>("--author");
            
            std::cout << "Committing:" << std::endl;
            std::cout << "  Message: " << msg << std::endl;
            std::cout << "  Author:  " << author << std::endl;
            std::cout << "  All:     " << (all ? "yes" : "no") << std::endl;
            
            if (args.positionalCount() > 0) {
                std::cout << "  Files:" << std::endl;
                for (size_t i = 0; i < args.positionalCount(); i++) {
                    std::cout << "    - " << args.getPositional<std::string>(i) << std::endl;
                }
            }
            
            return true;
        });
    
    // ==================== push command ====================
    parser.registerCommand("push")
        .description("Update remote refs along with associated objects")
        .flag("--force")
            .alias("-f")
            .argDescription("Force push")
        .flag("--dry-run")
            .argDescription("Show what would be pushed")
        .argumentOptional<std::string>("--remote")
            .defaultValue("origin")
            .alias("-r")
            .argDescription("Remote name")
        .argumentOptional<std::string>("--branch")
            .defaultValue("main")
            .alias("-b")
            .argDescription("Branch name")
        .execute([](CommandArgument& args) -> bool {
            std::string remote = args.get<std::string>("--remote");
            std::string branch = args.get<std::string>("--branch");
            bool force = args.has("--force");
            bool dryRun = args.has("--dry-run");
            
            std::cout << "Pushing to: " << remote << "/" << branch << std::endl;
            if (force) std::cout << "  Force: yes" << std::endl;
            if (dryRun) std::cout << "  (dry run)" << std::endl;
            else std::cout << "  (actual push)" << std::endl;
            
            return true;
        });
    
    // ==================== checkout command ====================
    parser.registerCommand("checkout")
        .description("Switch branches or restore working tree files")
        .flag("-b")
            .argDescription("Create a new branch")
        .positional<std::string>("branch")
            .argDescription("Branch name")
        .positionalOptional<std::string>("paths...")
            .argDescription("Paths to checkout")
        .execute([](CommandArgument& args) -> bool {
            bool newBranch = args.has("-b");
            std::string branch = args.getPositional<std::string>(0);
            
            std::cout << "Checking out: " << branch << std::endl;
            if (newBranch) {
                std::cout << "  Creating new branch" << std::endl;
            }
            
            if (args.positionalCount() > 0) {
                std::cout << "  Paths:" << std::endl;
                for (size_t i = 0; i < args.positionalCount(); i++) {
                    std::cout << "    - " << args.getPositional<std::string>(i) << std::endl;
                }
            }
            
            return true;
        });
    
    // ==================== alias ====================
    parser.registerAlias("ci", "commit");
    parser.registerAlias("co", "checkout");
    
    // ==================== help command ====================
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
        std::cerr << "Try: ./mygit help" << std::endl;
        return 1;
    }
}