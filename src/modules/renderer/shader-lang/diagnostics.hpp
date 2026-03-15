#pragma once

#include "shader-lang/ast.hpp"
#include <cstdio>
#include <cstdint>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace astralix {
#define ERROR_AT_LOC(msg, name, location)                                      \
  (std::string(msg) + ": '" + (name) + "' at " +                               \
   std::to_string((location).line) + ":" + std::to_string((location).col))

#define PUSH_ERROR(result, msg, name, location)                                \
  (result).errors.push_back(ERROR_AT_LOC(msg, name, location))

#define PUSH_UNDEFINED_IDENTIFIER(result, name, location)                      \
  PUSH_ERROR(result, "undefined identifier", name, location)

#define PUSH_UNRESOLVED_SYMBOL(result, name, location)                         \
  PUSH_ERROR(result, "unresolved symbol", name, location)

#define PUSH_CANNOT_OPEN_INCLUDE(result, path)                                 \
  (result).errors.push_back("cannot open include: '" + std::string(path) + "'")

#define PUSH_PREFIXED_INCLUDE_ERROR(result, include_path, error_msg)           \
  (result).errors.push_back("[" + std::string(include_path) + "] " +           \
                            std::string(error_msg))

#define PUSH_LOCATED_ERROR(errors, location, message, source)                  \
  ::astralix::push_located_error((errors), (location), (message), (source))

#define PUSH_INVALID_ATTRIBUTE(errors, attribute, target, source, ...)         \
  PUSH_LOCATED_ERROR(                                                          \
      (errors), (attribute).location,                                          \
      ::astralix::format_invalid_attribute_message(                            \
          ::astralix::attribute_kind_name((attribute).kind), (target),         \
          std::initializer_list<::astralix::AttributeKind>{__VA_ARGS__}),      \
      (source))

#define PUSH_INVALID_ANNOTATION(errors, annotation, target, source, ...)       \
  PUSH_LOCATED_ERROR(                                                          \
      (errors), (annotation).location,                                         \
      ::astralix::format_invalid_annotation_message(                           \
          ::astralix::annotation_kind_name((annotation).kind), (target),       \
          std::initializer_list<::astralix::AnnotationKind>{__VA_ARGS__}),     \
      (source))

#define PUSH_ATTRIBUTE_EXCLUSIVE_CONFLICT(errors, attribute, target, source,   \
                                          conflicting_kind)                    \
  PUSH_LOCATED_ERROR(                                                          \
      (errors), (attribute).location,                                          \
      ::astralix::format_attribute_exclusive_conflict_message(                 \
          ::astralix::attribute_kind_name((attribute).kind), (target),         \
          ::astralix::attribute_kind_name((conflicting_kind))),                \
      (source))

#define PUSH_ANNOTATION_EXCLUSIVE_CONFLICT(errors, annotation, target, source, \
                                           conflicting_kind)                   \
  PUSH_LOCATED_ERROR(                                                          \
      (errors), (annotation).location,                                         \
      ::astralix::format_annotation_exclusive_conflict_message(                \
          ::astralix::annotation_kind_name((annotation).kind), (target),       \
          ::astralix::annotation_kind_name((conflicting_kind))),               \
      (source))

inline std::string format_location_prefix(const SourceLocation &location) {
  return location.file.empty() ? (std::to_string(location.line) + ":" +
                                  std::to_string(location.col) + ": ")
                               : (std::string(location.file) + ":" +
                                  std::to_string(location.line) + ":" +
                                  std::to_string(location.col) + ": ");
}

inline std::string format_source_context(std::string_view source, uint32_t line,
                                         uint32_t col,
                                         uint32_t context_lines = 2) {
  std::vector<std::string_view> lines;
  size_t start = 0;
  for (size_t i = 0; i <= source.size(); ++i) {
    if (i == source.size() || source[i] == '\n') {
      lines.push_back(source.substr(start, i - start));
      start = i + 1;
    }
  }

  if (line == 0 || line > static_cast<uint32_t>(lines.size()))
    return {};

  uint32_t first = (line > context_lines + 1) ? (line - context_lines) : 1;
  uint32_t last =
      std::min(static_cast<uint32_t>(lines.size()), line + context_lines);

  int width = static_cast<int>(std::to_string(last).size());

  std::ostringstream out;

  for (uint32_t l = first; l <= last; ++l) {
    std::string line_number = std::to_string(l);

    out << std::string(width - static_cast<int>(line_number.size()), ' ')
        << line_number << " | " << lines[l - 1] << '\n';
    if (l == line) {
      out << std::string(width, ' ') << " | ";
      uint32_t pad = (col > 0) ? (col - 1) : 0;
      out << std::string(pad, ' ') << "^\n";
    }
  }
  return out.str();
}

inline std::string format_located_error(std::string message,
                                        const SourceLocation &location,
                                        std::string_view source = {}) {
  std::string error = format_location_prefix(location) + std::move(message);

  if (!source.empty()) {
    std::string context =
        format_source_context(source, location.line, location.col);
    if (!context.empty()) {
      error += '\n' + context;
    }
  }

  return error;
}

inline std::string format_invalid_attribute_message(
    std::string_view attribute_name, std::string_view target,
    std::string_view allowed_desc) {
  return "invalid attribute '" + std::string(attribute_name) + "' on " +
         std::string(target) + "; " + std::string(allowed_desc);
}

inline std::string
format_allowed_attributes(std::initializer_list<AttributeKind> allowed) {
  if (allowed.size() == 0) {
    return "no attributes are allowed";
  }

  std::string message = "only ";
  size_t index = 0;

  for (AttributeKind kind : allowed) {
    if (index > 0) {
      if (allowed.size() == 2) {
        message += " and ";
      } else if (index == allowed.size() - 1) {
        message += ", and ";
      } else {
        message += ", ";
      }
    }

    message += "'";
    message += attribute_kind_name(kind);
    message += "'";
    ++index;
  }

  message += allowed.size() == 1 ? " is allowed" : " are allowed";
  return message;
}

inline std::string format_invalid_attribute_message(
    std::string_view attribute_name, std::string_view target,
    std::initializer_list<AttributeKind> allowed) {
  return format_invalid_attribute_message(attribute_name, target,
                                          format_allowed_attributes(allowed));
}

inline std::string format_attribute_exclusive_conflict_message(
    std::string_view attribute_name, std::string_view target,
    std::string_view conflicting_name) {
  return "attribute '" + std::string(attribute_name) + "' conflicts with '" +
         std::string(conflicting_name) + "' on " + std::string(target);
}

inline std::string
format_allowed_annotations(std::initializer_list<AnnotationKind> allowed) {
  if (allowed.size() == 0) {
    return "no annotations are allowed";
  }

  std::string message = "only ";
  size_t index = 0;

  for (AnnotationKind kind : allowed) {
    if (index > 0) {
      if (allowed.size() == 2) {
        message += " and ";
      } else if (index == allowed.size() - 1) {
        message += ", and ";
      } else {
        message += ", ";
      }
    }

    message += "'";
    message += annotation_kind_name(kind);
    message += "'";
    ++index;
  }

  message += allowed.size() == 1 ? " is allowed" : " are allowed";
  return message;
}

inline std::string format_invalid_annotation_message(
    std::string_view annotation_name, std::string_view target,
    std::string_view allowed_desc) {
  return "invalid annotation '" + std::string(annotation_name) + "' on " +
         std::string(target) + "; " + std::string(allowed_desc);
}

inline std::string format_invalid_annotation_message(
    std::string_view annotation_name, std::string_view target,
    std::initializer_list<AnnotationKind> allowed) {
  return format_invalid_annotation_message(annotation_name, target,
                                           format_allowed_annotations(allowed));
}

inline std::string format_missing_required_annotation_message(
    std::string_view annotation_name, std::string_view target) {
  return "missing required annotation '" + std::string(annotation_name) +
         "' on " + std::string(target);
}

inline std::string format_annotation_exclusive_conflict_message(
    std::string_view annotation_name, std::string_view target,
    std::string_view conflicting_name) {
  return "annotation '" + std::string(annotation_name) + "' conflicts with '" +
         std::string(conflicting_name) + "' on " + std::string(target);
}

inline void push_located_error(std::vector<std::string> &errors,
                               const SourceLocation &location,
                               std::string message,
                               std::string_view source = {}) {
  errors.push_back(format_located_error(std::move(message), location, source));
}

inline void report_parser_stuck(std::string_view parser_name,
                                const Token &token) {
  std::fprintf(stderr, "[axslc] %.*s: stuck at token '%s' (%d) line %u\n",
               static_cast<int>(parser_name.size()), parser_name.data(),
               token.lexeme.c_str(), static_cast<int>(token.kind),
               token.location.line);
}

inline void report_parser_stuck(std::string_view parser_name,
                                std::string_view scope_name,
                                const Token &token) {
  std::fprintf(stderr, "[axslc] %.*s(%.*s): stuck at '%s' (%d) line %u\n",
               static_cast<int>(parser_name.size()), parser_name.data(),
               static_cast<int>(scope_name.size()), scope_name.data(),
               token.lexeme.c_str(), static_cast<int>(token.kind),
               token.location.line);
}

} // namespace astralix
