// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero-base/cli/CLI.h>
#include <xzero-base/cli/Flags.h>
#include <xzero-base/net/IPAddress.h>
#include <xzero-base/RuntimeError.h>
#include <xzero-base/Tokenizer.h>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace xzero {

CLI::CLI()
    : flagDefs_(),
      parametersEnabled_(false),
      parametersPlaceholder_(),
      parametersHelpText_() {
}

CLI& CLI::define(
    const std::string& longOpt,
    char shortOpt,
    bool required,
    FlagType type,
    const std::string& helpText,
    const std::string& valuePlaceholder,
    const std::string& defaultValue,
    std::function<void(const std::string&)> callback) {

  FlagDef fd;
  fd.type = type;
  fd.longOption = longOpt;
  fd.shortOption = shortOpt;
  fd.required = required;
  fd.helpText = helpText;
  fd.valuePlaceholder = valuePlaceholder;
  fd.defaultValue = defaultValue;
  fd.callback = callback;

  flagDefs_.emplace_back(fd);

  return *this;
}

CLI& CLI::defineString(
    const std::string& longOpt,
    char shortOpt,
    const std::string& helpText,
    std::function<void(const std::string&)> callback) {

  return define(longOpt, shortOpt, true, FlagType::String, helpText,
                "<string>", "", callback);
}

CLI& CLI::defineString(
    const std::string& longOpt,
    char shortOpt,
    const std::string& helpText,
    const std::string& defaultValue,
    std::function<void(const std::string&)> callback) {

  return define(longOpt, shortOpt, false, FlagType::String, helpText,
                "<string>", defaultValue, callback);
}

CLI& CLI::defineNumber(
    const std::string& longOpt,
    char shortOpt,
    const std::string& helpText,
    std::function<void(long int)> callback) {

  return define(
      longOpt, shortOpt, true, FlagType::Number, helpText,
      "<int>", "",
      [=](const std::string& value) {
        if (callback) {
          callback(std::stoi(value));
        }
      });
}

CLI& CLI::defineNumber(
    const std::string& longOpt,
    char shortOpt,
    const std::string& helpText,
    long int defaultValue,
    std::function<void(long int)> callback) {

  return define(
      longOpt, shortOpt, false, FlagType::Number, helpText,
      "<int>", std::to_string(defaultValue),
      [=](const std::string& value) {
        if (callback) {
          callback(std::stoi(value));
        }
      });
}

CLI& CLI::defineFloat(
    const std::string& longOpt,
    char shortOpt,
    const std::string& helpText,
    std::function<void(float)> callback) {

  return define(
      longOpt, shortOpt, true, FlagType::Float, helpText,
      "<float>", "",
      [=](const std::string& value) {
        if (callback) {
          callback(std::stof(value));
        }
      });
}

CLI& CLI::defineFloat(
    const std::string& longOpt,
    char shortOpt,
    const std::string& helpText,
    float defaultValue,
    std::function<void(float)> callback) {

  return define(
      longOpt, shortOpt, false, FlagType::Float, helpText,
      "<float>", std::to_string(defaultValue),
      [=](const std::string& value) {
        if (callback) {
          callback(std::stof(value));
        }
      });
}

CLI& CLI::defineIPAddress(
    const std::string& longOpt,
    char shortOpt,
    const std::string& helpText,
    std::function<void(const IPAddress&)> callback) {

  return define(
      longOpt, shortOpt, true, FlagType::IP, helpText,
      "<IP>", "",
      [=](const std::string& value) {
        if (callback) {
          callback(IPAddress(value));
        }
      });
}

CLI& CLI::defineIPAddress(
    const std::string& longOpt,
    char shortOpt,
    const std::string& helpText,
    const IPAddress& defaultValue,
    std::function<void(const IPAddress&)> callback) {

  return define(
      longOpt, shortOpt, false, FlagType::IP, helpText,
      "<IP>", defaultValue.str(),
      [=](const std::string& value) {
        if (callback) {
          callback(IPAddress(value));
        }
      });
}

CLI& CLI::defineBool(
    const std::string& longOpt,
    char shortOpt,
    const std::string& helpText,
    std::function<void(bool)> callback) {

  return define(
      longOpt, shortOpt, false, FlagType::Bool, helpText,
      "<bool>", "",
      [=](const std::string& value) {
        if (callback) {
          callback(value == "true");
        }
      });
}

CLI& CLI::enableParameters(const std::string& valuePlaceholder,
                                         const std::string& helpText) {
  parametersEnabled_ = true;
  parametersPlaceholder_ = valuePlaceholder;
  parametersHelpText_ = helpText;

  return *this;
}

const CLI::FlagDef* CLI::find(const std::string& longOption) const {
  for (const auto& flag: flagDefs_)
    if (flag.longOption == longOption)
      return &flag;

  return nullptr;
}

const CLI::FlagDef* CLI::find(char shortOption) const {
  for (const auto& flag: flagDefs_)
    if (flag.shortOption == shortOption)
      return &flag;

  return nullptr;
}

const CLI::FlagDef* CLI::require(const std::string& longOption) const {
  if (const FlagDef* fd = find(longOption))
    return fd;

  RAISE_EXCEPTION(CLI::UnknownOptionError); //"--" + longOption);
}

const CLI::FlagDef* CLI::require(char shortOption) const {
  if (const FlagDef* fd = find(shortOption))
    return fd;

  //char option[3] = { '-', shortOption, '\0' };
  RAISE_EXCEPTION(CLI::UnknownOptionError); // , option);
}

// -----------------------------------------------------------------------------
Flags CLI::evaluate(int argc, const char* argv[]) const {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i)
    args.push_back(argv[i]);

  return evaluate(args);
}

Flags CLI::evaluate(const std::vector<std::string>& args) const {
  Flags flags;

  auto call = [&](const std::string& name, const std::string& value) {
    const FlagDef* fd = require(name);
    if (fd->callback) {
      fd->callback(value);
    }
  };

  enum class ParsingState {
    Options,
    Parameters,
  };

  std::vector<std::string> params;
  ParsingState pstate = ParsingState::Options;
  size_t i = 0;

  while (i < args.size()) {
    std::string arg = args[i];
    if (pstate == ParsingState::Parameters) {
      params.push_back(arg);
      i++;
    } else if (arg == "--") {
      pstate = ParsingState::Parameters;
      i++;
    } else if (arg.size() > 2 && arg[0] == '-' && arg[1] == '-') {
      // longopt
      arg = arg.substr(2);
      size_t eq = arg.find('=');
      if (eq != arg.npos) { // --name=value
        std::string name = arg.substr(0, eq);
        std::string value = arg.substr(eq + 1);
        const FlagDef* fd = require(name);
        flags.set(name, value, FlagStyle::LongWithValue, fd->type);
        call(name, value);
        i++;
      } else { // --name [VALUE]
        const FlagDef* fd = require(arg);
        if (fd->type == FlagType::Bool) { // --name
          flags.set(arg, "true", FlagStyle::LongSwitch, fd->type);
          call(arg, "true");
          i++;
        } else { // --name VALUE
          i++;
          std::string name = arg;

          if (i >= args.size())
            // "--" + name
            RAISE_EXCEPTION(CLI::MissingOptionValueError);

          std::string value = args[i];
          i++;

          flags.set(name, value, FlagStyle::LongWithValue, fd->type);
          call(name, value);
        }
      }
    } else if (arg.size() > 1 && arg[0] == '-') {
      // shortopt
      arg = arg.substr(1);
      while (!arg.empty()) {
        const FlagDef* fd = find(arg[0]);
        if (fd == nullptr) { // option not found
          RAISE_EXCEPTION(CLI::UnknownOptionError); //"-" + arg.substr(0, 1));
        } else if (fd->type == FlagType::Bool) {
          flags.set(fd->longOption, "true",
                    FlagStyle::ShortSwitch, FlagType::Bool);
          call(fd->longOption, "true");
          arg = arg.substr(1);
          i++;
        } else if (arg.size() > 1) { // -fVALUE
          flags.set(fd->longOption, arg.substr(1),
                    FlagStyle::ShortSwitch, fd->type);
          call(fd->longOption, arg.substr(1));
          arg.clear();
          i++;
        } else { // -f VALUE
          std::string name = fd->longOption;
          i++;

          if (i >= args.size()) {
            //char option[3] = { '-', fd->shortOption, '\0' };
            RAISE_EXCEPTION(CLI::MissingOptionValueError); //, option);
          }

          arg.clear();
          std::string value = args[i];
          i++;

          if (!value.empty() && value[0] == '-') {
            //char option[3] = { '-', fd->shortOption, '\0' };
            RAISE_EXCEPTION(CLI::MissingOptionValueError); //, option);
          }

          flags.set(name, value, FlagStyle::ShortSwitch, fd->type);
          call(name, value);
        }
      }
    } else if (parametersEnabled_) {
      params.push_back(arg);
      i++;
    } else {
      // oops
      RAISE_EXCEPTION(CLI::UnknownOptionError); // args[i]
    }
  }

  flags.setParameters(params);

  // fill any missing default flags
  for (const FlagDef& fd: flagDefs_) {
    if (!fd.defaultValue.empty()) {
      if (!flags.isSet(fd.longOption)) {
        flags.set(fd.longOption, fd.defaultValue,
                  FlagStyle::LongWithValue, fd.type);
        call(fd.longOption, fd.defaultValue);
      }
    } else if (fd.type == FlagType::Bool) {
      if (!flags.isSet(fd.longOption)) {
        flags.set(fd.longOption, "false",
                  FlagStyle::LongWithValue, FlagType::Bool);
        call(fd.longOption, "false");
      }
    }
  }

  return flags;
}

// -----------------------------------------------------------------------------

std::string CLI::helpText(size_t width,
                          size_t helpTextOffset) const {
  std::stringstream sstr;

  for (const FlagDef& fd: flagDefs_)
    sstr << fd.makeHelpText(width, helpTextOffset);

  if (parametersEnabled_) {
    sstr << std::endl;

    size_t p = sstr.tellp();
    sstr << "    [--] " << parametersPlaceholder_;
    size_t column = static_cast<size_t>(sstr.tellp()) - p;

    if (column < helpTextOffset)
      sstr << std::setw(helpTextOffset - column) << ' ';
    else
      sstr << std::endl << std::setw(helpTextOffset) << ' ';

    sstr << parametersHelpText_ << std::endl;
  }

  return sstr.str();
}

static std::string wordWrap(
    const std::string& text,
    size_t currentWidth,
    size_t width,
    size_t indent) {

  //auto words = Tokenizer<std::string, std::string>::tokenize(text, " ");

  std::stringstream sstr;

  size_t i = 0;
  while (i < text.size()) {
    if (currentWidth >= width) {
      sstr << std::endl << std::setw(indent) << ' ';
      currentWidth = 0;
    }

    sstr << text[i];
    currentWidth++;
    i++;
  }

  return sstr.str();
}

// {{{ CLI::FlagDef
std::string CLI::FlagDef::makeHelpText(size_t width,
                                       size_t helpTextOffset) const {
  std::stringstream sstr;

  sstr << ' ';

  // short option
  if (shortOption)
    sstr << "-" << shortOption << ", ";
  else
    sstr << "    ";

  // long option
  sstr << "--" << longOption;

  // value placeholder
  if (type != FlagType::Bool) {
    if (!valuePlaceholder.empty()) {
      sstr << "=" << valuePlaceholder;
    } else {
      sstr << "=VALUE";
    }
  }

  // spacer
  size_t column = sstr.tellp();
  if (column < helpTextOffset) {
    sstr << std::setw(helpTextOffset - sstr.tellp()) << ' ';
  } else {
    sstr << std::endl << std::setw(helpTextOffset) << ' ';
    column = helpTextOffset;
  }

  // help output with default value hint.
  if (type != FlagType::Bool && !defaultValue.empty()) {
    sstr << wordWrap(helpText + " [" + defaultValue + "]",
                     column, width, helpTextOffset);
  } else {
    sstr << wordWrap(helpText, column, width, helpTextOffset);
  }

  sstr << std::endl;

  return sstr.str();
}
// }}}

}  // namespace xzero
