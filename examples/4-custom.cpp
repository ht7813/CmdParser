// ============================================================================
// Example 4: Custom Type Conversion
// ============================================================================
//
// Note: CmdParser v2 uses type erasure for argument storage.
// Custom types are not directly supported due to technical limitations.
// Instead, use std::string and convert manually.
//
// ============================================================================
#include "cmdparser.hpp"
#include <iostream>

using namespace cmdparser;

struct Point { int x, y; };

int main(int argc, char* argv[]) {
    CommandParser parser("custom");
    parser.registerCommand("draw")
        .argument<std::string>("--point")  // ← Use string
        .execute([](CommandArgument& args) -> bool {
            std::string str = args.get<std::string>("--point");
            
            // Manual conversion
            Point p;
            std::stringstream ss(str);
            char c;
            ss >> c >> p.x >> c >> p.y >> c;
            
            if (ss.fail()) {
                std::cerr << "Invalid point format. Use (x,y)" << std::endl;
                return false;
            }
            
            std::cout << "Point: (" << p.x << ", " << p.y << ")" << std::endl;
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