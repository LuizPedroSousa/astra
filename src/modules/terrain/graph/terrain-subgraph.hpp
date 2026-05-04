#pragma once

#include "execution-policy.hpp"
#include "subgraph-port.hpp"
#include "base.hpp"
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace astralix::terrain {

class TerrainSubgraph {
public:
  virtual ~TerrainSubgraph() = default;

  virtual void compile() = 0;
  virtual void process(const std::unordered_map<std::string, const SubgraphOutputData *> &inputs) = 0;

  virtual std::string_view name() const = 0;
  virtual ExecutionPolicy execution_policy() const = 0;

  virtual std::span<const SubgraphPort> input_ports() const = 0;
  virtual std::span<const SubgraphPort> output_ports() const = 0;

  virtual const SubgraphOutputData *get_output(const std::string &port_name) const = 0;

  void request_process() { m_demand_requested = true; }
  bool consume_process_request() {
    bool requested = m_demand_requested;
    m_demand_requested = false;
    return requested;
  }

  bool enabled = true;

private:
  bool m_demand_requested = false;
};

} // namespace astralix::terrain
