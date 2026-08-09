# Migrating from CmdParser v1 to v2

CmdParser v2 is a major release with breaking changes. This guide helps you migrate your code from v1 to v2.

## Why v2?

- **Better API design**: `.flag()` replaces `.argumentFlag<bool>()`
- **POSIX compliance**: Full `--` support
- **Cleaner semantics**: bool arguments are only for flags
- **More features**: Parameter aliases, default values, variadic positionals
- **Better error messages**: Unclosed quote detection

## Breaking Changes Summary

| v1 | v2 | Change Type |
|----|----|-------------|
| `.argumentFlag<bool>("-v")` | `.flag("-v")` | ❌ Breaking |
| `.argument<bool>("-v")` | `.flag("-v")` | ❌ Breaking |
| `.positional<bool>("flag")` | ❌ Removed | ❌ Breaking |
| `args.getPositional(0)` | `args.getPositional<std::string>(0)` | ❌ Breaking |
| No `--` support | Full `--` support | ✅ New |
| No unclosed quote detection | Unclosed quote throws exception | ✅ New |
| No parameter aliases | `.alias()` support | ✅ New |

## Step-by-Step Migration

### 1. Boolean Flags: `.argumentFlag<bool>` → `.flag`

```cpp
// v1
.argumentFlag<bool>("-v")
.argumentFlag<bool>("--verbose")

// v2
.flag("-v")
.flag("--verbose")
```

### 2. Remove `bool` from Other Argument Types

```cpp
// v1 (removed in v2)
.argument<bool>("-v")
.positional<bool>("flag")

// v2
.flag("-v")  // Only way to declare bool arguments
```

### 3. Positional Arguments with Type Conversion

```cpp
// v1
std::string str = args.getPositional(0);
int num = std::stoi(args.getPositional(1));

// v2
std::string str = args.getPositional<std::string>(0);
int num = args.getPositional<int>(1);
float f = args.getPositional<float>(2);
```

### 4. Optional Arguments with Default Values

```cpp
// v1 (no default value)
.argumentOptional<int>("--count")
int count = args.has("--count") ? args.get<int>("--count") : 1;

// v2 (with default value)
.argumentOptional<int>("--count", 1)
int count = args.get<int>("--count");  // Returns 1 if not provided
```

### 5. Parameter Aliases (NEW)

```cpp
// v2
.argument<std::string>("--message")
    .alias("-m")
    .alias("--msg")
// -m, --msg, --message all refer to the same argument
```

### 6. `--` Support (NEW)

```cpp
// v2 - all arguments after -- are treated as positional
parser.parse("commit -m \"msg\" -- --file-with-dash.txt");
// --file-with-dash.txt is a positional argument, not a flag
```

### 7. Unclosed Quote Detection (NEW)

```cpp
// v2 - throws exception
parser.parse("echo \"unclosed quote");  
// throws exceptions::InvalidCommandSyntax: "Quote never closes"
```

## Quick Migration Example

### v1 Code

```cpp
parser.registerCommand("push")
    .description("Push to remote")
    .argumentFlag<bool>("--force")
    .argument<std::string>("-r")
    .argumentOptional<std::string>("-m")
    .positional<std::string>("branch")
    .execute([](CommandArgument& args) -> bool {
        bool force = args.get<bool>("--force");
        std::string remote = args.get<std::string>("-r");
        std::string msg = args.has("-m") ? args.get<std::string>("-m") : "";
        std::string branch = args.getPositional(0);
        return true;
    });
```

### v2 Code (Migrated)

```cpp
parser.registerCommand("push")
    .description("Push to remote")
    .flag("--force")
        .alias("-f")                           // NEW
    .argument<std::string>("--remote")          // Renamed for clarity
        .alias("-r")                           // NEW
    .argumentOptional<std::string>("-m")
        .defaultValue("")                           // Default value
    .positional<std::string>("branch")
    .execute([](CommandArgument& args) -> bool {
        bool force = args.get<bool>("--force");
        std::string remote = args.get<std::string>("--remote");  // Use primary name
        std::string msg = args.get<std::string>("-m");          // Returns default
        std::string branch = args.getPositional<std::string>(0); // Type specified
        return true;
    });
```

## API Comparison Table

| Feature | v1 | v2 |
|---------|----|----|
| Boolean flag | `.argumentFlag<bool>("-v")` | `.flag("-v")` |
| Required argument | `.argument<T>("name")` | `.argument<T>("name")` |
| Optional argument | `.argumentOptional<T>("name")` | `.argumentOptional<T>("name")` |
| Optional with default | ❌ | `.argumentOptional<T>("name", default)` |
| Positional | `.positional<T>("name")` | `.positional<T>("name")` |
| Positional (typed) | `args.getPositional(0)` | `args.getPositional<T>(0)` |
| Positional (variadic) | ❌ | `.positional<T>("name...")` |
| Parameter alias | ❌ | `.alias("alt")` |
| `--` support | ❌ | ✅ |
| Unclosed quote check | ❌ | ✅ |
| `get<T>()` | ✅ | ✅ |
| `has()` | ✅ | ✅ |

## Breaking Changes Checklist

- [ ] Replace all `.argumentFlag<bool>` with `.flag`
- [ ] Remove all `.argument<bool>` and `.positional<bool>`
- [ ] Add explicit type to all `getPositional()` calls
- [ ] (Optional) Add default values to `argumentOptional`
- [ ] (Optional) Add parameter aliases with `.alias()`
- [ ] (Optional) Use `--` in command line for files starting with `-`

## Need Help?

- Check the [examples](examples/) directory for v2 examples
- Open an issue if you encounter any migration issues
- For v1 users, stick with the v1 branch