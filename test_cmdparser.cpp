#include "cmdparser.hpp"

#include <gtest/gtest.h>
#include <sstream>

using namespace cmdparser;

// ============================================================================
// Test Fixtures
// ============================================================================

class CmdParserTest : public ::testing::Test {
protected:
    CommandParser parser{"testcli"};

    void SetUp() override {
        // Register a basic command for testing
        parser.registerCommand("greet")
            .description("Greet someone")
            .argument<std::string>("-n")
            .argumentFlag<bool>("-u")
            .execute([](CommandArgument& args) -> bool {
                return true;
            });

        parser.registerCommand("add")
            .description("Add numbers")
            .argument<int>("-a")
            .argument<int>("-b")
            .execute([](CommandArgument& args) -> bool {
                return true;
            });

        parser.registerCommand("echo")
            .description("Echo positional args")
            .positional<std::string>("msg")
            .positionalOptional<std::string>("extra")
            .execute([](CommandArgument& args) -> bool {
                return true;
            });
    }
};

// ============================================================================
// Command Registration Tests
// ============================================================================

TEST_F(CmdParserTest, RegisterCommand) {
    EXPECT_NO_THROW({
        parser.registerCommand("test");
    });
}

TEST_F(CmdParserTest, RegisterDuplicateCommand) {
    EXPECT_THROW({
        parser.registerCommand("greet");
    }, exceptions::DuplicateCommand);
}

TEST_F(CmdParserTest, RegisterAlias) {
    EXPECT_NO_THROW({
        parser.registerAlias("gr", "greet");
    });
}

TEST_F(CmdParserTest, RegisterAliasToUnknownCommand) {
    EXPECT_THROW({
        parser.registerAlias("xyz", "nonexistent");
    }, exceptions::UnknownCommand);
}

// ============================================================================
// Basic Parsing Tests
// ============================================================================

TEST_F(CmdParserTest, ParseKnownCommand) {
    const char* argv[] = {"testcli", "greet", "-n", "World"};
    EXPECT_TRUE(parser.parse(4, const_cast<char**>(argv)));
}

TEST_F(CmdParserTest, ParseUnknownCommand) {
    const char* argv[] = {"testcli", "unknown"};
    EXPECT_THROW({
        parser.parse(2, const_cast<char**>(argv));
    }, exceptions::UnknownCommand);
}

TEST_F(CmdParserTest, ParseEmptyCommand) {
    EXPECT_THROW({
        parser.parse("");
    }, exceptions::InvalidCommandSyntax);
}

TEST_F(CmdParserTest, ParseCommandLineString) {
    EXPECT_TRUE(parser.parse("greet -n Alice"));
}

// ============================================================================
// Argument Retrieval Tests
// ============================================================================

class ArgRetrievalTest : public CmdParserTest {
protected:
    bool stringArgCaptured = false;
    std::string capturedString;
    bool boolArgCaptured = false;
    bool capturedBool = false;

    void SetUp() override {
        parser.registerCommand("capture")
            .argument<std::string>("-s")
            .argumentFlag<bool>("-f")
            .execute([this](CommandArgument& args) -> bool {
                if (args.has("-s")) {
                    stringArgCaptured = true;
                    capturedString = args.get<std::string>("-s");
                }
                if (args.has("-f")) {
                    boolArgCaptured = true;
                    capturedBool = args.get<bool>("-f");
                }
                return true;
            });
    }
};

TEST_F(ArgRetrievalTest, GetStringArgument) {
    const char* argv[] = {"testcli", "capture", "-s", "test_value"};
    parser.parse(4, const_cast<char**>(argv));
    
    EXPECT_TRUE(stringArgCaptured);
    EXPECT_EQ(capturedString, "test_value");
}

TEST_F(ArgRetrievalTest, GetBoolFlag) {
    const char* argv[] = {"testcli", "capture", "-f"};
    parser.parse(3, const_cast<char**>(argv));
    
    EXPECT_TRUE(boolArgCaptured);
    EXPECT_TRUE(capturedBool);
}

TEST_F(ArgRetrievalTest, ArgumentNotFound) {
    const char* argv[] = {"testcli", "capture"};
    parser.parse(2, const_cast<char**>(argv));
    
    EXPECT_FALSE(stringArgCaptured);
}

TEST_F(ArgRetrievalTest, HasArgument) {
    const char* argv[] = {"testcli", "capture", "-s", "value"};
    parser.parse(3, const_cast<char**>(argv));
    
    // The handler should have access to args
}

// ============================================================================
// Positional Arguments Tests
// ============================================================================

TEST_F(CmdParserTest, PositionalArgumentParsing) {
    bool executed = false;
    parser.registerCommand("pos")
        .positional<std::string>("name")
        .execute([&executed](CommandArgument& args) -> bool {
            EXPECT_EQ(args.getPositional(0), "testfile");
            EXPECT_EQ(args.positionalCount(), 1);
            executed = true;
            return true;
        });

    const char* argv[] = {"testcli", "pos", "testfile"};
    EXPECT_TRUE(parser.parse(3, const_cast<char**>(argv)));
    EXPECT_TRUE(executed);
}

TEST_F(CmdParserTest, PositionalArgumentIndexOutOfRange) {
    bool executed = false;
    parser.registerCommand("pos2")
        .positional<std::string>("name")
        .execute([&executed](CommandArgument& args) -> bool {
            EXPECT_THROW({
                args.getPositional(10);
            }, exceptions::InvalidCommandSyntax);
            executed = true;
            return true;
        });

    const char* argv[] = {"testcli", "pos2", "testfile"};
    parser.parse(3, const_cast<char**>(argv));
    EXPECT_TRUE(executed);
}

// ============================================================================
// Quoted String Tests
// ============================================================================

TEST_F(CmdParserTest, ParseQuotedString) {
    bool executed = false;
    parser.registerCommand("quote")
        .argument<std::string>("-m")
        .execute([&executed](CommandArgument& args) -> bool {
            EXPECT_EQ(args.get<std::string>("-m"), "hello world");
            executed = true;
            return true;
        });

    EXPECT_TRUE(parser.parse("quote -m \"hello world\""));
    EXPECT_TRUE(executed);
}

TEST_F(CmdParserTest, ParseSingleQuotedString) {
    bool executed = false;
    parser.registerCommand("quote2")
        .argument<std::string>("-m")
        .execute([&executed](CommandArgument& args) -> bool {
            EXPECT_EQ(args.get<std::string>("-m"), "single quoted");
            executed = true;
            return true;
        });

    EXPECT_TRUE(parser.parse("quote2 -m 'single quoted'"));
    EXPECT_TRUE(executed);
}

// ============================================================================
// Command Alias Tests
// ============================================================================

TEST_F(CmdParserTest, ParseViaAlias) {
    bool executed = false;
    parser.registerCommand("aliastest")
        .argument<std::string>("-n")
        .execute([&executed](CommandArgument& args) -> bool {
            EXPECT_EQ(args.get<std::string>("-n"), "via_alias");
            executed = true;
            return true;
        });

    parser.registerAlias("at", "aliastest");
    
    const char* argv[] = {"testcli", "at", "-n", "via_alias"};
    EXPECT_TRUE(parser.parse(4, const_cast<char**>(argv)));
    EXPECT_TRUE(executed);
}

// ============================================================================
// Exception Tests
// ============================================================================

TEST_F(CmdParserTest, DuplicateCommandException) {
    CommandParser p("test");
    p.registerCommand("cmd");
    EXPECT_THROW({
        p.registerCommand("cmd");
    }, exceptions::DuplicateCommand);
}

TEST_F(CmdParserTest, UnknownCommandException) {
    CommandParser p("test");
    const char* argv[] = {"test", "unknown"};
    EXPECT_THROW({
        p.parse(2, const_cast<char**>(argv));
    }, exceptions::UnknownCommand);
}

TEST_F(CmdParserTest, ArgumentTypeMismatch) {
    bool executed = false;
    parser.registerCommand("typecheck")
        .argument<int>("-num")
        .execute([&executed](CommandArgument& args) -> bool {
            // Try to get as wrong type
            EXPECT_THROW({
                args.get<std::string>("-num");
            }, exceptions::ArgumentTypeMismatch);
            executed = true;
            return true;
        });

    const char* argv[] = {"testcli", "typecheck", "-num", "42"};
    parser.parse(4, const_cast<char**>(argv));
    EXPECT_TRUE(executed);
}

// ============================================================================
// ArgumentValue Tests
// ============================================================================

TEST(ArgumentValueTest, DefaultConstruction) {
    ArgumentValue av;
    EXPECT_TRUE(av.isEmpty());
}

TEST(ArgumentValueTest, StringConstruction) {
    ArgumentValue av(std::string("test"));
    EXPECT_FALSE(av.isEmpty());
    EXPECT_EQ(av.as<std::string>(), "test");
    EXPECT_EQ(av.toString(), "test");
}

TEST(ArgumentValueTest, IntConstruction) {
    ArgumentValue av(42);
    EXPECT_FALSE(av.isEmpty());
    EXPECT_EQ(av.as<int>(), 42);
}

TEST(ArgumentValueTest, MoveConstruction) {
    ArgumentValue av1(std::string("move_test"));
    ArgumentValue av2(std::move(av1));
    
    EXPECT_TRUE(av1.isEmpty());
    EXPECT_EQ(av2.as<std::string>(), "move_test");
}

TEST(ArgumentValueTest, TypeQuery) {
    ArgumentValue intAv(100);
    ArgumentValue strAv(std::string("test"));
    
    EXPECT_EQ(intAv.type(), std::type_index(typeid(int)));
    EXPECT_EQ(strAv.type(), std::type_index(typeid(std::string)));
}

// ============================================================================
// CommandArgument Tests
// ============================================================================

TEST(CommandArgumentTest, SetAndGet) {
    CommandArgument args;
    args.set("name", std::string("John"));
    
    EXPECT_TRUE(args.has("name"));
    EXPECT_EQ(args.get<std::string>("name"), "John");
}

TEST(CommandArgumentTest, AddPositional) {
    CommandArgument args;
    args.addPositional("first");
    args.addPositional("second");
    
    EXPECT_EQ(args.positionalCount(), 2);
    EXPECT_EQ(args.getPositional(0), "first");
    EXPECT_EQ(args.getPositional(1), "second");
}

TEST(CommandArgumentTest, GetArgumentNames) {
    CommandArgument args;
    args.set("a", std::string("1"));
    args.set("b", std::string("2"));
    
    auto names = args.getArgumentNames();
    EXPECT_EQ(names.size(), 2);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(IntegrationTest, FullWorkflow) {
    CommandParser parser("git");
    
    // Register commit command
    parser.registerCommand("commit")
        .description("Commit changes")
        .argument<std::string>("-m")
        .argumentFlag<bool>("-a")
        .execute([](CommandArgument& args) -> bool {
            EXPECT_EQ(args.get<std::string>("-m"), "Initial commit");
            EXPECT_FALSE(args.has("-a"));
            return true;
        });
    
    // Register push command
    parser.registerCommand("push")
        .description("Push changes")
        .argumentFlag<bool>("--force")
        .positional<std::string>("branch")
        .execute([](CommandArgument& args) -> bool {
            EXPECT_EQ(args.getPositional(0), "main");
            EXPECT_TRUE(args.has("--force"));
            return true;
        });
    
    // Test commit
    EXPECT_TRUE(parser.parse("commit -m \"Initial commit\""));
    
    // Test push with force
    EXPECT_TRUE(parser.parse("push --force main"));
    
    // Test alias
    parser.registerAlias("ci", "commit");
    EXPECT_TRUE(parser.parse("ci -m \"Quick fix\""));
}

TEST(IntegrationTest, ShellCommandParsing) {
    CommandParser parser("shell");
    
    bool executed = false;
    parser.registerCommand("run")
        .argument<std::string>("-c")
        .execute([&executed](CommandArgument& args) -> bool {
            EXPECT_EQ(args.get<std::string>("-c"), "echo hello world");
            executed = true;
            return true;
        });
    
    EXPECT_TRUE(parser.parse("run -c \"echo hello world\""));
    EXPECT_TRUE(executed);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
