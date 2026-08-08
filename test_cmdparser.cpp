#include "cmdparser.hpp"

#include <gtest/gtest.h>
#include <sstream>
#include <cmath>

using namespace cmdparser;

// ============================================================================
// Test Fixtures
// ============================================================================

class CmdParserTest : public ::testing::Test
{
protected:
    CommandParser parser{"testcli"};

    void SetUp() override
    {
        // Register a basic command for testing
        parser.registerCommand("greet")
            .description("Greet someone")
            .argument<std::string>("-n")
            .argumentFlag<bool>("-u")
            .execute([](CommandArgument &args) -> bool
                     { return true; });

        parser.registerCommand("add")
            .description("Add numbers")
            .argument<int>("-a")
            .argument<int>("-b")
            .execute([](CommandArgument &args) -> bool
                     { return true; });

        parser.registerCommand("echo")
            .description("Echo positional args")
            .positional<std::string>("msg")
            .positionalOptional<std::string>("extra")
            .execute([](CommandArgument &args) -> bool
                     { return true; });
    }
};

// ============================================================================
// Command Registration Tests
// ============================================================================

TEST_F(CmdParserTest, RegisterCommand)
{
    EXPECT_NO_THROW({
        parser.registerCommand("test");
    });
}

TEST_F(CmdParserTest, RegisterDuplicateCommand)
{
    EXPECT_THROW({ parser.registerCommand("greet"); }, exceptions::DuplicateCommand);
}

TEST_F(CmdParserTest, RegisterAlias)
{
    EXPECT_NO_THROW({
        parser.registerAlias("gr", "greet");
    });
}

TEST_F(CmdParserTest, RegisterAliasToUnknownCommand)
{
    EXPECT_THROW({ parser.registerAlias("xyz", "nonexistent"); }, exceptions::UnknownCommand);
}

// ============================================================================
// Basic Parsing Tests
// ============================================================================

TEST_F(CmdParserTest, ParseKnownCommand)
{
    const char *argv[] = {"testcli", "greet", "-n", "World"};
    EXPECT_TRUE(parser.parse(4, const_cast<char **>(argv)));
}

TEST_F(CmdParserTest, ParseUnknownCommand)
{
    const char *argv[] = {"testcli", "unknown"};
    EXPECT_THROW({ parser.parse(2, const_cast<char **>(argv)); }, exceptions::UnknownCommand);
}

TEST_F(CmdParserTest, ParseEmptyCommand)
{
    EXPECT_THROW({ parser.parse(""); }, exceptions::InvalidCommandSyntax);
}

TEST_F(CmdParserTest, ParseCommandLineString)
{
    EXPECT_TRUE(parser.parse("greet -n Alice"));
}

// ============================================================================
// Argument Retrieval Tests
// ============================================================================

class ArgRetrievalTest : public CmdParserTest
{
protected:
    bool stringArgCaptured = false;
    std::string capturedString;
    bool boolArgCaptured = false;
    bool capturedBool = false;

    void SetUp() override
    {
        parser.registerCommand("capture")
            .argumentOptional<std::string>("-s")
            .argumentFlag<bool>("-f")
            .execute([this](CommandArgument &args) -> bool
                     {
                if (args.has("-s")) {
                    stringArgCaptured = true;
                    capturedString = args.get<std::string>("-s");
                }
                if (args.has("-f")) {
                    boolArgCaptured = true;
                    capturedBool = args.get<bool>("-f");
                }
                return true; });
    }
};

TEST_F(ArgRetrievalTest, GetStringArgument)
{
    const char *argv[] = {"testcli", "capture", "-s", "test_value"};
    parser.parse(4, const_cast<char **>(argv));

    EXPECT_TRUE(stringArgCaptured);
    EXPECT_EQ(capturedString, "test_value");
}

TEST_F(ArgRetrievalTest, GetBoolFlag)
{
    const char *argv[] = {"testcli", "capture", "-f"};
    parser.parse(3, const_cast<char **>(argv));

    EXPECT_TRUE(boolArgCaptured);
    EXPECT_TRUE(capturedBool);
}

TEST_F(ArgRetrievalTest, ArgumentNotFound)
{
    const char *argv[] = {"testcli", "capture"};
    parser.parse(2, const_cast<char **>(argv));

    EXPECT_FALSE(stringArgCaptured);
}

TEST_F(ArgRetrievalTest, HasArgument)
{
    const char *argv[] = {"testcli", "capture", "-s", "value"};
    parser.parse(3, const_cast<char **>(argv));

    // The handler should have access to args
}

// ============================================================================
// Positional Arguments Tests
// ============================================================================

TEST_F(CmdParserTest, PositionalArgumentParsing)
{
    bool executed = false;
    parser.registerCommand("pos")
        .positional<std::string>("name")
        .execute([&executed](CommandArgument &args) -> bool
                 {
            EXPECT_EQ(args.getPositional(0), "testfile");
            EXPECT_EQ(args.positionalCount(), 1);
            executed = true;
            return true; });

    const char *argv[] = {"testcli", "pos", "testfile"};
    EXPECT_TRUE(parser.parse(3, const_cast<char **>(argv)));
    EXPECT_TRUE(executed);
}

TEST_F(CmdParserTest, PositionalArgumentIndexOutOfRange)
{
    bool executed = false;
    parser.registerCommand("pos2")
        .positional<std::string>("name")
        .execute([&executed](CommandArgument &args) -> bool
                 {
            EXPECT_THROW({
                args.getPositional(10);
            }, exceptions::InvalidCommandSyntax);
            executed = true;
            return true; });

    const char *argv[] = {"testcli", "pos2", "testfile"};
    parser.parse(3, const_cast<char **>(argv));
    EXPECT_TRUE(executed);
}

// ============================================================================
// Quoted String Tests
// ============================================================================

TEST_F(CmdParserTest, ParseQuotedString)
{
    bool executed = false;
    parser.registerCommand("quote")
        .argument<std::string>("-m")
        .execute([&executed](CommandArgument &args) -> bool
                 {
            EXPECT_EQ(args.get<std::string>("-m"), "hello world");
            executed = true;
            return true; });

    EXPECT_TRUE(parser.parse("quote -m \"hello world\""));
    EXPECT_TRUE(executed);
}

TEST_F(CmdParserTest, ParseSingleQuotedString)
{
    bool executed = false;
    parser.registerCommand("quote2")
        .argument<std::string>("-m")
        .execute([&executed](CommandArgument &args) -> bool
                 {
            EXPECT_EQ(args.get<std::string>("-m"), "single quoted");
            executed = true;
            return true; });

    EXPECT_TRUE(parser.parse("quote2 -m 'single quoted'"));
    EXPECT_TRUE(executed);
}

// ============================================================================
// Command Alias Tests
// ============================================================================

TEST_F(CmdParserTest, ParseViaAlias)
{
    bool executed = false;
    parser.registerCommand("aliastest")
        .argument<std::string>("-n")
        .execute([&executed](CommandArgument &args) -> bool
                 {
            EXPECT_EQ(args.get<std::string>("-n"), "via_alias");
            executed = true;
            return true; });

    parser.registerAlias("at", "aliastest");

    const char *argv[] = {"testcli", "at", "-n", "via_alias"};
    EXPECT_TRUE(parser.parse(4, const_cast<char **>(argv)));
    EXPECT_TRUE(executed);
}

// ============================================================================
// Exception Tests
// ============================================================================

TEST_F(CmdParserTest, DuplicateCommandException)
{
    CommandParser p("test");
    p.registerCommand("cmd");
    EXPECT_THROW({ p.registerCommand("cmd"); }, exceptions::DuplicateCommand);
}

TEST_F(CmdParserTest, UnknownCommandException)
{
    CommandParser p("test");
    const char *argv[] = {"test", "unknown"};
    EXPECT_THROW({ p.parse(2, const_cast<char **>(argv)); }, exceptions::UnknownCommand);
}

TEST_F(CmdParserTest, ArgumentTypeMismatch)
{
    bool executed = false;
    parser.registerCommand("typecheck")
        .argument<int>("-num")
        .execute([&executed](CommandArgument &args) -> bool
                 {
            // Try to get as wrong type
            EXPECT_THROW({
                args.get<std::string>("-num");
            }, exceptions::ArgumentTypeMismatch);
            executed = true;
            return true; });

    const char *argv[] = {"testcli", "typecheck", "-num", "42"};
    parser.parse(4, const_cast<char **>(argv));
    EXPECT_TRUE(executed);
}

// ============================================================================
// ArgumentValue Tests
// ============================================================================

TEST(ArgumentValueTest, DefaultConstruction)
{
    ArgumentValue av;
    EXPECT_TRUE(av.isEmpty());
}

TEST(ArgumentValueTest, StringConstruction)
{
    ArgumentValue av(std::string("test"));
    EXPECT_FALSE(av.isEmpty());
    EXPECT_EQ(av.as<std::string>(), "test");
    EXPECT_EQ(av.toString(), "test");
}

TEST(ArgumentValueTest, IntConstruction)
{
    ArgumentValue av(42);
    EXPECT_FALSE(av.isEmpty());
    EXPECT_EQ(av.as<int>(), 42);
}

TEST(ArgumentValueTest, MoveConstruction)
{
    ArgumentValue av1(std::string("move_test"));
    ArgumentValue av2(std::move(av1));

    EXPECT_TRUE(av1.isEmpty());
    EXPECT_EQ(av2.as<std::string>(), "move_test");
}

TEST(ArgumentValueTest, TypeQuery)
{
    ArgumentValue intAv(100);
    ArgumentValue strAv(std::string("test"));

    EXPECT_EQ(intAv.type(), std::type_index(typeid(int)));
    EXPECT_EQ(strAv.type(), std::type_index(typeid(std::string)));
}

// ============================================================================
// CommandArgument Tests
// ============================================================================

TEST(CommandArgumentTest, SetAndGet)
{
    CommandArgument args;
    args.set("name", std::string("John"));

    EXPECT_TRUE(args.has("name"));
    EXPECT_EQ(args.get<std::string>("name"), "John");
}

TEST(CommandArgumentTest, AddPositional)
{
    CommandArgument args;
    args.addPositional("first");
    args.addPositional("second");

    EXPECT_EQ(args.positionalCount(), 2);
    EXPECT_EQ(args.getPositional(0), "first");
    EXPECT_EQ(args.getPositional(1), "second");
}

TEST(CommandArgumentTest, GetArgumentNames)
{
    CommandArgument args;
    args.set("a", std::string("1"));
    args.set("b", std::string("2"));

    auto names = args.getArgumentNames();
    EXPECT_EQ(names.size(), 2);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(IntegrationTest, FullWorkflow)
{
    CommandParser parser("git");
    std::string expectedCommitMessage = "Initial commit";

    // Register commit command
    parser.registerCommand("commit")
        .description("Commit changes")
        .argument<std::string>("-m")
        .argumentFlag<bool>("-a")
        .execute([&expectedCommitMessage](CommandArgument &args) -> bool
                 {
            EXPECT_EQ(args.get<std::string>("-m"), expectedCommitMessage);
            EXPECT_FALSE(args.has("-a"));
            return true; });

    // Register push command
    parser.registerCommand("push")
        .description("Push changes")
        .argumentFlag<bool>("--force")
        .positional<std::string>("branch")
        .execute([](CommandArgument &args) -> bool
                 {
            EXPECT_EQ(args.getPositional(0), "main");
            EXPECT_TRUE(args.has("--force"));
            return true; });

    // Test commit
    EXPECT_TRUE(parser.parse("commit -m \"Initial commit\""));

    // Test push with force
    EXPECT_TRUE(parser.parse("push --force main"));

    // Test alias
    expectedCommitMessage = "Quick fix";
    parser.registerAlias("ci", "commit");
    EXPECT_TRUE(parser.parse("ci -m \"Quick fix\""));
}

TEST(IntegrationTest, ShellCommandParsing)
{
    CommandParser parser("shell");

    bool executed = false;
    parser.registerCommand("run")
        .argument<std::string>("-c")
        .execute([&executed](CommandArgument &args) -> bool
                 {
            EXPECT_EQ(args.get<std::string>("-c"), "echo hello world");
            executed = true;
            return true; });

    EXPECT_TRUE(parser.parse("run -c \"echo hello world\""));
    EXPECT_TRUE(executed);
}

// ============================================================================
// ShortOption Tests
// ============================================================================

TEST(ShortOptionTest, ExpandShortOptions) {
    CommandParser parser("shorttest");
    bool executed = false;
    parser.registerCommand("test")
        .argumentFlag<bool>("-a")
        .argumentFlag<bool>("-b")
        .argumentFlag<bool>("-c")
        .execute([&executed](CommandArgument &args) -> bool
        {
            EXPECT_TRUE(args.has("-a"));
            EXPECT_TRUE(args.has("-b"));
            EXPECT_TRUE(args.has("-c"));
            executed = true;
            return true; 
    });
    EXPECT_TRUE(parser.parse("test -abc"));
    EXPECT_TRUE(executed);
}

TEST(ShortOptionTest, ExpandShortOptionsWithStringArgument) {
    CommandParser parser("shorttest2");
    bool executed = false;
    parser.registerCommand("test")
        .argumentFlag<bool>("-a")
        .argumentFlag<bool>("-b")
        .argument<std::string>("-c")
        .execute([&executed](CommandArgument &args) -> bool
        {
            EXPECT_TRUE(args.has("-a"));
            EXPECT_TRUE(args.has("-b"));
            EXPECT_TRUE(args.has("-c"));
            EXPECT_EQ(args.get<std::string>("-c"), "Short Option Expand Test");
            executed = true;
            return true; 
    });
    EXPECT_TRUE(parser.parse("test -abc \"Short Option Expand Test\""));
    EXPECT_TRUE(executed);
}

TEST(ShortOptionTest, ExpandShortOptionsWithIntArgument) {
    CommandParser parser("shorttest2");
    bool executed = false;
    parser.registerCommand("test")
        .argumentFlag<bool>("-a")
        .argumentFlag<bool>("-b")
        .argument<int>("-c")
        .execute([&executed](CommandArgument &args) -> bool
        {
            EXPECT_TRUE(args.has("-a"));
            EXPECT_TRUE(args.has("-b"));
            EXPECT_TRUE(args.has("-c"));
            EXPECT_EQ(args.get<int>("-c"), 114514);
            executed = true;
            return true; 
    });
    EXPECT_TRUE(parser.parse("test -bac 114514"));
    EXPECT_TRUE(executed);
}

TEST(ShortOptionTest, ExpandShortOptionWithAppendedValue) {
    CommandParser parser("system");
    std::string expectedUserName = "amy";
    parser.registerCommand("login")
        .argument<std::string>("-u")
        .execute([&expectedUserName](CommandArgument &args) -> bool
        {
            EXPECT_TRUE(args.has("-u"));
            EXPECT_EQ(args.get<std::string>("-u"), expectedUserName);
            return true;
        });
    EXPECT_TRUE(parser.parse("login -uamy"));
}

TEST(ShortOptionTest, LoginWithCombinedShortOptionsAndAppendedValue) {
    CommandParser parser("system");
    bool i_flag = false;
    std::string expectedUserName = "very_long_user_name";

    parser.registerCommand("login")
    .argumentFlag<bool>("-i")
    .argument<std::string>("-u")
    .execute([&i_flag, &expectedUserName](CommandArgument &args) -> bool
    {
        EXPECT_TRUE(args.has("-i"));
        EXPECT_TRUE(args.has("-u"));
        EXPECT_TRUE(args.get<bool>("-i"));
        EXPECT_EQ(args.get<std::string>("-u"), expectedUserName);
        i_flag = true;
        return true;
    });

    EXPECT_TRUE(parser.parse("login -iuvery_long_user_name"));
    EXPECT_TRUE(i_flag);
}

TEST(ShortOptionTest, ExpandComplexShortOptionsWithMixedFlagsAndIntArgument) {
    CommandParser parser("shorttest2");
    bool executed = false;

    parser.registerCommand("test")
    .argumentFlag<bool>("-a")
    .argumentFlag<bool>("-b")
    .argument<int>("-i")
    .argumentFlag<bool>("-c")
    .argumentFlag<bool>("-d")
    .argumentFlag<bool>("-e")
    .execute([&executed](CommandArgument &args) -> bool
    {
        // 验证所有 flag 都存在
        EXPECT_TRUE(args.has("-a"));
        EXPECT_TRUE(args.has("-b"));
        EXPECT_TRUE(args.has("-c"));
        EXPECT_TRUE(args.has("-d"));
        EXPECT_TRUE(args.has("-e"));

        // 验证 -i 存在且有正确的值
        EXPECT_TRUE(args.has("-i"));
        EXPECT_EQ(args.get<int>("-i"), 42);

        executed = true;
        return true;
    });

    EXPECT_TRUE(parser.parse("test -abi 42 -cde"));
    EXPECT_TRUE(executed);
}

// ============================================================================
// 测试：类型自动转换
// ============================================================================

TEST(TypeConversionTest, ParseIntValues) {
    cmdparser::CommandParser parser("test");
    bool executed = false;
    int intValue = 0;

    parser.registerCommand("test")
        .argument<int>("-n")
        .execute([&executed, &intValue](cmdparser::CommandArgument& args) -> bool {
            intValue = args.get<int>("-n");
            executed = true;
            return true;
        });

    EXPECT_TRUE(parser.parse("test -n 42"));
    EXPECT_EQ(intValue, 42);

    EXPECT_TRUE(parser.parse("test -n -114514"));
    EXPECT_EQ(intValue, -114514);

    EXPECT_TRUE(parser.parse("test -n 0"));
    EXPECT_EQ(intValue, 0);
}

TEST(TypeConversionTest, ParseLongValues) {
    cmdparser::CommandParser parser("test");
    bool executed = false;
    long longValue = 0;

    parser.registerCommand("test")
        .argument<long>("-l")
        .execute([&executed, &longValue](cmdparser::CommandArgument& args) -> bool {
            longValue = args.get<long>("-l");
            executed = true;
            return true;
        });

    EXPECT_TRUE(parser.parse("test -l 1234567890"));
    EXPECT_EQ(longValue, 1234567890L);

    // On some platform, long = 32-bit
    EXPECT_TRUE(parser.parse("test -l -2147483647"));
    EXPECT_EQ(longValue, -2147483647L);
}

TEST(TypeConversionTest, ParseUnsignedLongValues) {
    cmdparser::CommandParser parser("test");
    bool executed = false;
    unsigned long longValue = 0;

    parser.registerCommand("test")
        .argument<unsigned long>("-l")
        .execute([&executed, &longValue](cmdparser::CommandArgument& args) -> bool {
            longValue = args.get<unsigned long>("-l");
            executed = true;
            return true;
        });

    // On some platform, unsigned long = 32-bit (too)
    EXPECT_TRUE(parser.parse("test -l 2147483650"));
    EXPECT_EQ(longValue, 2147483650UL);

    // Same as above
    EXPECT_TRUE(parser.parse("test -l 4294967294"));
    EXPECT_EQ(longValue, 4294967294UL);
}

TEST(TypeConversionTest, ParseLongLongValues) {
    cmdparser::CommandParser parser("test");
    bool executed = false;
    long long longValue = 0;

    parser.registerCommand("test")
        .argument<long long>("-l")
        .execute([&executed, &longValue](cmdparser::CommandArgument& args) -> bool {
            longValue = args.get<long long>("-l");
            executed = true;
            return true;
        });

    EXPECT_TRUE(parser.parse("test -l 999999999999"));
    EXPECT_EQ(longValue, 999999999999LL);

    EXPECT_TRUE(parser.parse("test -l -1999999999999"));
    EXPECT_EQ(longValue, -1999999999999LL);
}

TEST(TypeConversionTest, ParseUnsignedLongLongValues) {
    cmdparser::CommandParser parser("test");
    bool executed = false;
    unsigned long long longValue = 0;

    parser.registerCommand("test")
        .argument<unsigned long long>("-l")
        .execute([&executed, &longValue](cmdparser::CommandArgument& args) -> bool {
            longValue = args.get<unsigned long long>("-l");
            executed = true;
            return true;
        });

    EXPECT_TRUE(parser.parse("test -l 9223372036854775999"));
    EXPECT_EQ(longValue, 9223372036854775999ULL);

    EXPECT_TRUE(parser.parse("test -l 18446744073709551614"));
    EXPECT_EQ(longValue, 18446744073709551614ULL);
}

TEST(TypeConversionTest, ParseFloatValues) {
    cmdparser::CommandParser parser("test");
    bool executed = false;
    float floatValue = 0.0f;

    parser.registerCommand("test")
        .argument<float>("-f")
        .execute([&executed, &floatValue](cmdparser::CommandArgument& args) -> bool {
            floatValue = args.get<float>("-f");
            executed = true;
            return true;
        });

    EXPECT_TRUE(parser.parse("test -f 3.14159"));
    EXPECT_FLOAT_EQ(floatValue, 3.14159f);

    EXPECT_TRUE(parser.parse("test -f -2.71828"));
    EXPECT_FLOAT_EQ(floatValue, -2.71828f);

    EXPECT_TRUE(parser.parse("test -f 1.0e-5"));
    EXPECT_FLOAT_EQ(floatValue, 1.0e-5f);
}

TEST(TypeConversionTest, ParseDoubleValues) {
    cmdparser::CommandParser parser("test");
    bool executed = false;
    double doubleValue = 0.0;

    parser.registerCommand("test")
        .argument<double>("-d")
        .execute([&executed, &doubleValue](cmdparser::CommandArgument& args) -> bool {
            doubleValue = args.get<double>("-d");
            executed = true;
            return true;
        });

    EXPECT_TRUE(parser.parse("test -d 3.141592653589793"));
    EXPECT_DOUBLE_EQ(doubleValue, 3.141592653589793);

    EXPECT_TRUE(parser.parse("test -d -2.718281828459045"));
    EXPECT_DOUBLE_EQ(doubleValue, -2.718281828459045);

    EXPECT_TRUE(parser.parse("test -d 1.234e-10"));
    EXPECT_DOUBLE_EQ(doubleValue, 1.234e-10);
}

TEST(TypeConversionTest, ParseMixedTypes) {
    cmdparser::CommandParser parser("test");
    bool executed = false;
    int intVal = 0;
    float floatVal = 0.0f;
    std::string strVal;
    bool boolVal = false;

    parser.registerCommand("test")
        .argument<int>("-i")
        .argument<float>("-f")
        .argument<std::string>("-s")
        .argumentFlag<bool>("-b")
        .execute([&executed, &intVal, &floatVal, &strVal, &boolVal]
                 (cmdparser::CommandArgument& args) -> bool {
            intVal = args.get<int>("-i");
            floatVal = args.get<float>("-f");
            strVal = args.get<std::string>("-s");
            boolVal = args.has("-b");
            executed = true;
            return true;
        });

    EXPECT_TRUE(parser.parse("test -i 42 -f 3.14 -s hello -b"));
    EXPECT_EQ(intVal, 42);
    EXPECT_FLOAT_EQ(floatVal, 3.14f);
    EXPECT_EQ(strVal, "hello");
    EXPECT_TRUE(boolVal);

    EXPECT_TRUE(parser.parse("test -i -100 -f -0.001 -s world"));
    EXPECT_EQ(intVal, -100);
    EXPECT_FLOAT_EQ(floatVal, -0.001f);
    EXPECT_EQ(strVal, "world");
    EXPECT_FALSE(boolVal);
}

TEST(TypeConversionTest, InvalidConversionThrows) {
    cmdparser::CommandParser parser("test");
    bool executed = false;

    parser.registerCommand("test")
        .argument<int>("-i")
        .execute([&executed](cmdparser::CommandArgument& args) -> bool {
            // 不应该到达这里
            executed = true;
            return true;
        });

    // 无效的整数
    EXPECT_THROW(parser.parse("test -i abc"), cmdparser::exceptions::InvalidCommandSyntax);

    // 无效的浮点数
    parser.registerCommand("test2")
        .argument<float>("-f")
        .execute([&executed](cmdparser::CommandArgument& args) -> bool {
            executed = true;
            return true;
        });

    EXPECT_THROW(parser.parse("test2 -f not-a-number"), cmdparser::exceptions::InvalidCommandSyntax);
    EXPECT_THROW(parser.parse("test2 -f NaN"), cmdparser::exceptions::InvalidCommandSyntax);
    EXPECT_THROW(parser.parse("test2 -f nullptr"), cmdparser::exceptions::InvalidCommandSyntax);
}

TEST(TypeConversionTest, ShortOptionWithAppendedTypeConversion) {
    cmdparser::CommandParser parser("test");
    bool executed = false;
    int intVal = 0;

    parser.registerCommand("test")
        .argument<int>("-n")
        .argumentFlag<bool>("-v")
        .execute([&executed, &intVal](cmdparser::CommandArgument& args) -> bool {
            intVal = args.get<int>("-n");
            EXPECT_TRUE(args.has("-v"));
            executed = true;
            return true;
        });

    // -vn42 展开为 -v (bool) + -n (int) 且值紧跟在后面
    EXPECT_TRUE(parser.parse("test -vn42"));
    EXPECT_EQ(intVal, 42);
    EXPECT_TRUE(executed);

    // 再次测试其他值
    executed = false;
    EXPECT_TRUE(parser.parse("test -vn -114514"));
    EXPECT_EQ(intVal, -114514);
    EXPECT_TRUE(executed);
}

TEST(TypeConversionTest, ShortOptionGroupWithMultipleTypes) {
    cmdparser::CommandParser parser("test");
    bool executed = false;
    bool flagA = false;
    int intVal = 0;
    float floatVal = 0.0f;
    bool flagB = false;

    parser.registerCommand("test")
        .argumentFlag<bool>("-a")
        .argument<int>("-i")
        .argument<float>("-f")
        .argumentFlag<bool>("-b")
        .execute([&executed, &flagA, &intVal, &floatVal, &flagB]
                 (cmdparser::CommandArgument& args) -> bool {
            flagA = args.get<bool>("-a");
            intVal = args.get<int>("-i");
            floatVal = args.get<float>("-f");
            flagB = args.has("-b");
            executed = true;
            return true;
        });

    // -aif3.14b 展开为 -a (bool) + -i (int) + -f (float) + -b (bool)
    EXPECT_TRUE(parser.parse("test -ai42 -bf3.14"));
    EXPECT_TRUE(flagA);
    EXPECT_EQ(intVal, 42);
    EXPECT_FLOAT_EQ(floatVal, 3.14f);
    EXPECT_TRUE(flagB);
    EXPECT_TRUE(executed);

    // 测试负数
    executed = false;
    EXPECT_TRUE(parser.parse("test -ai-114514 -bf-2.718"));
    EXPECT_TRUE(flagA);
    EXPECT_EQ(intVal, -114514);
    EXPECT_FLOAT_EQ(floatVal, -2.718f);
    EXPECT_TRUE(flagB);
    EXPECT_TRUE(executed);
}

// ============================================================================
// 压力测试
// ============================================================================

class StressTest : public ::testing::Test
{
protected:
    CommandParser parser{"stresscli"};

    void SetUp() override
    {
        // 注册一个包含大量标志和少量带参数选项的命令
        parser.registerCommand("stress")
            .argumentFlag<bool>("-a")
            .argumentFlag<bool>("-b")
            .argumentFlag<bool>("-c")
            .argumentFlag<bool>("-d")
            .argumentFlag<bool>("-e")
            .argumentFlag<bool>("-f")
            .argumentFlag<bool>("-g")
            .argumentFlag<bool>("-h")
            .argument<int>("-i")          // i 需要参数
            .argumentFlag<bool>("-j")
            .argumentFlag<bool>("-k")
            .argumentFlag<bool>("-l")
            .argumentFlag<bool>("-m")
            .argumentFlag<bool>("-n")
            .argumentFlag<bool>("-o")
            .argumentFlag<bool>("-p")
            .argumentFlag<bool>("-q")
            .argumentFlag<bool>("-r")
            .argumentFlag<bool>("-s")
            .argumentFlag<bool>("-t")
            .argumentFlag<bool>("-u")
            .argumentFlag<bool>("-v")
            .argument<std::string>("-w")  // w 需要参数
            .argumentFlag<bool>("-x")
            .argumentFlag<bool>("-y")
            .argumentFlag<bool>("-z")
            .execute([](CommandArgument &args) -> bool {
                // 验证所有标志
                EXPECT_TRUE(args.has("-a"));
                EXPECT_TRUE(args.has("-b"));
                EXPECT_TRUE(args.has("-c"));
                EXPECT_TRUE(args.has("-d"));
                EXPECT_TRUE(args.has("-e"));
                EXPECT_TRUE(args.has("-f"));
                EXPECT_TRUE(args.has("-g"));
                EXPECT_TRUE(args.has("-h"));
                EXPECT_TRUE(args.has("-i"));
                EXPECT_EQ(args.get<int>("-i"), 12345);
                EXPECT_TRUE(args.has("-j"));
                EXPECT_TRUE(args.has("-k"));
                EXPECT_TRUE(args.has("-l"));
                EXPECT_TRUE(args.has("-m"));
                EXPECT_TRUE(args.has("-n"));
                EXPECT_TRUE(args.has("-o"));
                EXPECT_TRUE(args.has("-p"));
                EXPECT_TRUE(args.has("-q"));
                EXPECT_TRUE(args.has("-r"));
                EXPECT_TRUE(args.has("-s"));
                EXPECT_TRUE(args.has("-t"));
                EXPECT_TRUE(args.has("-u"));
                EXPECT_TRUE(args.has("-v"));
                EXPECT_TRUE(args.has("-w"));
                EXPECT_EQ(args.get<std::string>("-w"), "hello");
                EXPECT_TRUE(args.has("-x"));
                EXPECT_TRUE(args.has("-y"));
                EXPECT_TRUE(args.has("-z"));
                return true;
            });
    }
};

// 测试1：带参数选项使用追加值形式（-i12345 -whello）
TEST_F(StressTest, ShortOptionsWithAppendedValues)
{
    // 像 GNU getopt 那样：-abcdefghi12345 -jklmnopqrstuvwhello -xyz
    // i 的参数是 12345，w 的参数是 hello
    EXPECT_TRUE(parser.parse("stress -abcdefghi12345 -jklmnopqrstuvwhello -xyz"));
}

// 测试2：带参数选项使用空格分隔
TEST_F(StressTest, ShortOptionsWithSpaceSeparatedValues)
{
    EXPECT_TRUE(parser.parse("stress -abcdefgh -i 12345 -jklmnopqrstuv -w hello -xyz"));
}

// 测试3：短选项分组 + 追加值混合
TEST_F(StressTest, MixedShortOptionGroups)
{
    EXPECT_TRUE(parser.parse("stress -abc -defghi12345 -jklmnopqrstuvwhello -xyz"));
    EXPECT_TRUE(parser.parse("stress -abcdef -ghi12345 -jklmnopqrstuvwhello -xyz"));
}

// 测试4：只有标志，没有带参数选项
TEST_F(StressTest, FlagsOnlyShortOptions)
{
    // 注册一个全部是标志的命令
    parser.registerCommand("flags_only")
        .argumentFlag<bool>("-a")
        .argumentFlag<bool>("-b")
        .argumentFlag<bool>("-c")
        .argumentFlag<bool>("-d")
        .argumentFlag<bool>("-e")
        .argumentFlag<bool>("-f")
        .argumentFlag<bool>("-g")
        .argumentFlag<bool>("-h")
        .argumentFlag<bool>("-i")
        .argumentFlag<bool>("-j")
        .argumentFlag<bool>("-k")
        .argumentFlag<bool>("-l")
        .argumentFlag<bool>("-m")
        .argumentFlag<bool>("-n")
        .argumentFlag<bool>("-o")
        .argumentFlag<bool>("-p")
        .argumentFlag<bool>("-q")
        .argumentFlag<bool>("-r")
        .argumentFlag<bool>("-s")
        .argumentFlag<bool>("-t")
        .execute([](CommandArgument &args) -> bool {
            // 验证所有标志
            EXPECT_TRUE(args.has("-a"));
            EXPECT_TRUE(args.has("-b"));
            EXPECT_TRUE(args.has("-c"));
            EXPECT_TRUE(args.has("-d"));
            EXPECT_TRUE(args.has("-e"));
            EXPECT_TRUE(args.has("-f"));
            EXPECT_TRUE(args.has("-g"));
            EXPECT_TRUE(args.has("-h"));
            EXPECT_TRUE(args.has("-i"));
            EXPECT_TRUE(args.has("-j"));
            EXPECT_TRUE(args.has("-k"));
            EXPECT_TRUE(args.has("-l"));
            EXPECT_TRUE(args.has("-m"));
            EXPECT_TRUE(args.has("-n"));
            EXPECT_TRUE(args.has("-o"));
            EXPECT_TRUE(args.has("-p"));
            EXPECT_TRUE(args.has("-q"));
            EXPECT_TRUE(args.has("-r"));
            EXPECT_TRUE(args.has("-s"));
            EXPECT_TRUE(args.has("-t"));
            return true;
        });

    // 可以一口气展开所有标志（因为没有带参数的选项）
    EXPECT_TRUE(parser.parse("flags_only -abcdefghijklmnopqrst"));
}

// ============================================================================
// 压力测试：大量短选项的多种组合方式
// ============================================================================

TEST_F(StressTest, VariousShortOptionCombinations)
{
    // 各种合理的组合方式
    EXPECT_TRUE(parser.parse("stress -abcdefghi12345 -jklmnopqrstuvwhello -xyz"));
    EXPECT_TRUE(parser.parse("stress -abcdefghi12345 -jklmnopqrstuvwhello -xyz"));
    EXPECT_TRUE(parser.parse("stress -abcdefgh -i12345 -jklmnopqrstuv -whello -xyz"));
    EXPECT_TRUE(parser.parse("stress -abc -def -ghi12345 -jkl -mnopqrstuvwhello -xyz"));
}

// ============================================================================
// 压力测试：大量参数（每个都带独立参数）
// ============================================================================

TEST_F(StressTest, ManyArgumentsWithValues)
{
    bool executed = false;
    auto builder = parser.registerCommand("many_vals");
    
    // 20个带参数的短选项
    for (int i = 0; i < 20; ++i) {
        char c = 'a' + i;
        std::string argName = "-" + std::string(1, c);
        builder.argument<int>(argName);
    }
    
    builder.execute([&executed](CommandArgument &args) -> bool {
        for (int i = 0; i < 20; ++i) {
            char c = 'a' + i;
            std::string argName = "-" + std::string(1, c);
            EXPECT_TRUE(args.has(argName));
            EXPECT_EQ(args.get<int>(argName), i * 100);
        }
        executed = true;
        return true;
    });

    // 每个选项独立指定（不能连写，因为每个都要参数）
    std::string cmd = "many_vals";
    for (int i = 0; i < 20; ++i) {
        char c = 'a' + i;
        cmd += " -" + std::string(1, c) + " " + std::to_string(i * 100);
    }
    
    EXPECT_TRUE(parser.parse(cmd));
    EXPECT_TRUE(executed);
}

// ============================================================================
// 压力测试：混合标志和带参数选项（复杂场景）
// ============================================================================

TEST_F(StressTest, ComplexMixedFlagsAndValues)
{
    bool executed = false;
    parser.registerCommand("complex")
        .argumentFlag<bool>("-a")
        .argumentFlag<bool>("-b")
        .argument<int>("-c")
        .argumentFlag<bool>("-d")
        .argumentFlag<bool>("-e")
        .argument<std::string>("-f")
        .argumentFlag<bool>("-g")
        .argument<int>("-h")
        .argumentFlag<bool>("-i")
        .argumentFlag<bool>("-j")
        .argumentFlag<bool>("-k")
        .argument<std::string>("-l")
        .argumentFlag<bool>("-m")
        .execute([&executed](CommandArgument &args) -> bool {
            EXPECT_TRUE(args.has("-a"));
            EXPECT_TRUE(args.has("-b"));
            EXPECT_TRUE(args.has("-c"));
            EXPECT_EQ(args.get<int>("-c"), 42);
            EXPECT_TRUE(args.has("-d"));
            EXPECT_TRUE(args.has("-e"));
            EXPECT_TRUE(args.has("-f"));
            EXPECT_EQ(args.get<std::string>("-f"), "hello");
            EXPECT_TRUE(args.has("-g"));
            EXPECT_TRUE(args.has("-h"));
            EXPECT_EQ(args.get<int>("-h"), 100);
            EXPECT_TRUE(args.has("-i"));
            EXPECT_TRUE(args.has("-j"));
            EXPECT_TRUE(args.has("-k"));
            EXPECT_TRUE(args.has("-l"));
            EXPECT_EQ(args.get<std::string>("-l"), "world");
            EXPECT_TRUE(args.has("-m"));
            executed = true;
            return true;
        });

    // 合理的组合：带参数选项使用追加值或空格分隔
    EXPECT_TRUE(parser.parse("complex -abc42 -defhello -gh100 -ijklworld -m"));
    EXPECT_TRUE(parser.parse("complex -ab -c42 -de -fhello -g -h100 -ijk -lworld -m"));
    EXPECT_TRUE(parser.parse("complex -a -b -c 42 -d -e -f hello -g -h 100 -i -j -k -l world -m"));
}

// ============================================================================
// 压力测试：最多标志数量（全部是标志，没有参数）
// ============================================================================

TEST_F(StressTest, MaximumFlagsOnly)
{
    bool executed = false;
    auto builder = parser.registerCommand("max_flags");
    
    std::string allFlags;
    // 注册 52 个标志（a-z + A-Z）
    for (int i = 0; i < 52; ++i) {
        char c = (i < 26) ? ('a' + i) : ('A' + i - 26);
        std::string flagName = "-" + std::string(1, c);
        builder.argumentFlag<bool>(flagName);
        allFlags += c;
    }
    
    builder.execute([&executed, &allFlags](CommandArgument &args) -> bool {
        for (char c : allFlags) {
            std::string flagName = "-" + std::string(1, c);
            EXPECT_TRUE(args.has(flagName));
        }
        executed = true;
        return true;
    });

    // 一次性展开所有标志（因为都是标志，没有需要参数的）
    std::string cmd = "max_flags -" + allFlags;
    EXPECT_TRUE(parser.parse(cmd));
    EXPECT_TRUE(executed);
}

// ============================================================================
// 压力测试：重复解析大量命令
// ============================================================================

TEST_F(StressTest, RepeatedParsingStress)
{
    bool executed = false;
    parser.registerCommand("repeat")
        .argument<int>("-n")
        .argument<std::string>("-s")
        .execute([&executed](CommandArgument &args) -> bool {
            EXPECT_TRUE(args.has("-n"));
            EXPECT_TRUE(args.has("-s"));
            executed = true;
            return true;
        });

    // 连续解析 1000 次
    for (int i = 0; i < 1000; ++i) {
        executed = false;
        std::string cmd = "repeat -n" + std::to_string(i) + " -stest_" + std::to_string(i);
        EXPECT_TRUE(parser.parse(cmd));
        EXPECT_TRUE(executed);
    }
}

// ============================================================================
// 压力测试：长选项和短选项混合
// ============================================================================

TEST_F(StressTest, MixedLongShortStress)
{
    bool executed = false;
    parser.registerCommand("mix_stress")
        .argumentFlag<bool>("-a")
        .argumentFlag<bool>("--flag-a")
        .argument<int>("-b")
        .argument<int>("--flag-b")
        .argumentFlag<bool>("-c")
        .argumentFlag<bool>("--flag-c")
        .argument<std::string>("-d")
        .argument<std::string>("--flag-d")
        .argumentFlag<bool>("-e")
        .argumentFlag<bool>("--flag-e")
        .argument<double>("-f")
        .argument<double>("--flag-f")
        .execute([&executed](CommandArgument &args) -> bool {
            EXPECT_TRUE(args.has("-a"));
            EXPECT_TRUE(args.has("--flag-a"));
            EXPECT_TRUE(args.has("-b"));
            EXPECT_EQ(args.get<int>("-b"), 10);
            EXPECT_TRUE(args.has("--flag-b"));
            EXPECT_EQ(args.get<int>("--flag-b"), 20);
            EXPECT_TRUE(args.has("-c"));
            EXPECT_TRUE(args.has("--flag-c"));
            EXPECT_TRUE(args.has("-d"));
            EXPECT_EQ(args.get<std::string>("-d"), "short");
            EXPECT_TRUE(args.has("--flag-d"));
            EXPECT_EQ(args.get<std::string>("--flag-d"), "long");
            EXPECT_TRUE(args.has("-e"));
            EXPECT_TRUE(args.has("--flag-e"));
            EXPECT_TRUE(args.has("-f"));
            EXPECT_DOUBLE_EQ(args.get<double>("-f"), 1.1);
            EXPECT_TRUE(args.has("--flag-f"));
            EXPECT_DOUBLE_EQ(args.get<double>("--flag-f"), 2.2);
            executed = true;
            return true;
        });

    EXPECT_TRUE(parser.parse("mix_stress -a --flag-a -b10 --flag-b 20 -c --flag-c -dshort --flag-d long -e --flag-e -f1.1 --flag-f 2.2"));
}

// ============================================================================
// 压力测试：大量命令别名
// ============================================================================

TEST_F(StressTest, ManyAliases)
{
    parser.registerCommand("base_cmd")
        .argument<std::string>("-n")
        .execute([](CommandArgument &args) -> bool {
            EXPECT_EQ(args.get<std::string>("-n"), "test");
            return true;
        });

    // 注册 100 个别名
    for (int i = 0; i < 100; ++i) {
        std::string alias = "a" + std::to_string(i);
        EXPECT_NO_THROW({
            parser.registerAlias(alias, "base_cmd");
        });
    }

    // 测试所有别名
    for (int i = 0; i < 100; ++i) {
        std::string alias = "a" + std::to_string(i);
        std::string cmd = alias + " -ntest";
        EXPECT_TRUE(parser.parse(cmd));
    }
}

// ============================================================================
// 压力测试：超长命令行字符串
// ============================================================================

TEST_F(StressTest, VeryLongCommandLine)
{
    bool executed = false;
    std::string longString(100000, 'x');  // 100KB 字符串
    parser.registerCommand("long_arg")
        .argument<std::string>("-s")
        .execute([&executed, &longString](CommandArgument &args) -> bool {
            EXPECT_EQ(args.get<std::string>("-s"), longString);
            executed = true;
            return true;
        });

    std::string cmd = "long_arg -s" + longString;
    EXPECT_TRUE(parser.parse(cmd));
    EXPECT_TRUE(executed);
}

// ============================================================================
// 压力测试：位置参数和短选项混合
// ============================================================================

TEST_F(StressTest, PositionalsWithShortOptions)
{
    bool executed = false;
    parser.registerCommand("pos_stress")
        .argumentFlag<bool>("-a")
        .argumentFlag<bool>("-b")
        .argument<int>("-c")
        .argumentFlag<bool>("-d")
        .argument<std::string>("-e")
        .positional<std::string>("file1")
        .positional<std::string>("num1")
        .positionalOptional<std::string>("file2")
        .execute([&executed](CommandArgument &args) -> bool {
            EXPECT_TRUE(args.has("-a"));
            EXPECT_TRUE(args.has("-b"));
            EXPECT_TRUE(args.has("-c"));
            EXPECT_EQ(args.get<int>("-c"), 42);
            EXPECT_TRUE(args.has("-d"));
            EXPECT_TRUE(args.has("-e"));
            EXPECT_EQ(args.get<std::string>("-e"), "hello");
            EXPECT_EQ(args.getPositional(0), "input.txt");
            EXPECT_EQ(args.getPositional(1), "123");
            EXPECT_EQ(args.positionalCount(), 2);
            executed = true;
            return true;
        });

    EXPECT_TRUE(parser.parse("pos_stress -abc42 -dehello input.txt 123"));
    EXPECT_TRUE(parser.parse("pos_stress input.txt -abc42 -d -ehello 123"));
    EXPECT_TRUE(parser.parse("pos_stress -abc42 input.txt -dehello 123"));
}

// ============================================================================
// 压力测试：特殊字符和 Unicode
// ============================================================================

TEST_F(StressTest, SpecialCharactersAndUnicode)
{
    bool executed = false;
    std::string specialStr = "!@#$%^&*()_+-=[]{}|;:,.<>?/`~";
    std::string unicodeStr = "你好世界 🌍 こんにちは 안녕하세요";
    
    parser.registerCommand("special_unicode")
        .argument<std::string>("-s")
        .argument<std::string>("-u")
        .execute([&executed, &specialStr, &unicodeStr](CommandArgument &args) -> bool {
            EXPECT_EQ(args.get<std::string>("-s"), specialStr);
            EXPECT_EQ(args.get<std::string>("-u"), unicodeStr);
            executed = true;
            return true;
        });

    // 使用追加值形式，避免引号转义问题
    std::string cmd = "special_unicode -s" + specialStr + " -u \"" + unicodeStr + "\"";
    EXPECT_TRUE(parser.parse(cmd));
    EXPECT_TRUE(executed);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
