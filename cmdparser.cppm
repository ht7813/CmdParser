// C++ 23+ Modules for CmdParser
// SPDX-License-Identifier: MIT

module;
#include "cmdparser.hpp"
export module cmdparser;

export using cmdparser::CommandParser;
export using cmdparser::CommandArgument;
export using cmdparser::CommandRegister;
export namespace cmdparser::exceptions {
    using ::cmdparser::exceptions::InvalidCommandSyntax;
    using ::cmdparser::exceptions::UnknownCommand;
    using ::cmdparser::exceptions::DuplicateCommand;
    using ::cmdparser::exceptions::DuplicateArgument;
    using ::cmdparser::exceptions::DuplicateArgumentAlias;
    using ::cmdparser::exceptions::ArgumentTypeMismatch;
    using ::cmdparser::exceptions::ArgumentNotFound;
}
