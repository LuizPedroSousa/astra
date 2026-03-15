#pragma once
#include "shader-lang/ast.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace astralix {

struct LinkResult {
  std::vector<ASTNode> all_nodes;
  std::unordered_map<StageKind, std::unordered_set<std::string>> uniform_usage;
  std::vector<std::string> errors;
  bool ok() const { return errors.empty(); }
};

struct ScopeInfo {
  std::unordered_set<std::string> global_uniforms;
  std::unordered_map<StageKind, std::unordered_set<std::string>> stage_uniforms;
};

class Linker {
public:
  LinkResult link(Program &program, const std::vector<ASTNode> &nodes,
                  std::string_view base_path = {});

private:
  void scan_uniform_usage(
      NodeID nid, StageKind stage, const std::vector<ASTNode> &nodes,
      const ScopeInfo &scopes,
      std::unordered_map<StageKind, std::unordered_set<std::string>>
          &uniform_usage);

  void collect_locals(NodeID nid, const std::vector<ASTNode> &nodes,
                      std::unordered_set<std::string> &local_vars);
};

} // namespace astralix
