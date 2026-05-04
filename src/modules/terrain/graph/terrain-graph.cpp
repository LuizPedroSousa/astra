#include "terrain-graph.hpp"
#include "assert.hpp"
#include "log.hpp"
#include "trace.hpp"
#include <unordered_map>
#include <unordered_set>

namespace astralix::terrain {

namespace {

const char *queue_hint_name(QueueHint queue_hint) {
  switch (queue_hint) {
    case QueueHint::Main:
      return "Main";
    case QueueHint::AsyncCompute:
      return "AsyncCompute";
    case QueueHint::Background:
      return "Background";
  }

  return "Unknown";
}

const char *trigger_name(ExecutionTrigger trigger) {
  switch (trigger) {
    case ExecutionTrigger::OneShot:
      return "OneShot";
    case ExecutionTrigger::PerFrame:
      return "PerFrame";
    case ExecutionTrigger::OnDemand:
      return "OnDemand";
  }

  return "Unknown";
}

} // namespace

void TerrainGraph::add_subgraph(Scope<TerrainSubgraph> subgraph) {
  m_subgraphs.push_back(std::move(subgraph));
  m_compiled = false;
}

void TerrainGraph::compile() {
  ASTRA_PROFILE_N("TerrainGraph::compile");

  if (auto *jobs = JobSystem::get(); jobs != nullptr) {
    for (const auto &[index, handle] : m_running_jobs) {
      (void)index;
      jobs->wait(handle);
    }
  }
  m_running_jobs.clear();

  size_t count = m_subgraphs.size();

  for (auto &subgraph : m_subgraphs) {
    subgraph->compile();
  }

  std::unordered_map<std::string, size_t> output_producers;
  for (size_t index = 0; index < count; ++index) {
    for (const auto &port : m_subgraphs[index]->output_ports()) {
      output_producers[std::string(m_subgraphs[index]->name()) + "." + port.name] = index;
    }
  }

  std::vector<std::unordered_set<size_t>> dependencies(count);
  for (size_t index = 0; index < count; ++index) {
    for (const auto &port : m_subgraphs[index]->input_ports()) {
      auto iterator = output_producers.find(port.name);
      if (iterator != output_producers.end() && iterator->second != index) {
        dependencies[index].insert(iterator->second);
      }
    }
  }

  m_dependencies.assign(count, {});
  m_execution_order.clear();
  m_execution_order.reserve(count);
  std::vector<bool> visited(count, false);
  std::vector<bool> in_stack(count, false);

  auto topological_visit = [&](auto &self, size_t node) -> void {
    if (visited[node]) {
      return;
    }

    ASTRA_ENSURE(in_stack[node],
                  "TerrainGraph: cyclic subgraph dependency involving '%.*s'",
                  static_cast<int>(m_subgraphs[node]->name().size()),
                  m_subgraphs[node]->name().data());

    in_stack[node] = true;

    for (size_t dependency : dependencies[node]) {
      self(self, dependency);
    }

    in_stack[node] = false;
    visited[node] = true;
    m_execution_order.push_back(node);
  };

  for (size_t index = 0; index < count; ++index) {
    m_dependencies[index].assign(
        dependencies[index].begin(), dependencies[index].end()
    );
    topological_visit(topological_visit, index);
  }

  m_one_shot_executed.assign(count, false);
  m_compiled = true;
  LOG_DEBUG("[TerrainGraph] compiled", "subgraphs=", count);
}

void TerrainGraph::process() {
  ASTRA_PROFILE_N("TerrainGraph::process");

  ASTRA_ENSURE(!m_compiled, "TerrainGraph::process called before compile()");

  std::unordered_map<std::string, const SubgraphOutputData *> available_outputs;
  auto *jobs = JobSystem::get();

  auto register_outputs = [&](size_t index) {
    auto &subgraph = m_subgraphs[index];
    for (const auto &port : subgraph->output_ports()) {
      const auto *output = subgraph->get_output(port.name);
      if (output == nullptr) {
        continue;
      }

      available_outputs[std::string(subgraph->name()) + "." + port.name] =
          output;
    }
  };

  auto wait_for_subgraph = [&](size_t index) {
    auto running_it = m_running_jobs.find(index);
    if (running_it == m_running_jobs.end()) {
      return;
    }

    if (jobs != nullptr) {
      LOG_DEBUG(
          "[TerrainGraph] waiting for async subgraph",
          m_subgraphs[index]->name(),
          "job_id=", running_it->second.id
      );
      jobs->wait(running_it->second);
    }

    LOG_DEBUG("[TerrainGraph] async subgraph complete", m_subgraphs[index]->name());
    m_running_jobs.erase(running_it);
    register_outputs(index);
  };

  for (size_t index : m_execution_order) {
    wait_for_subgraph(index);

    auto &subgraph = m_subgraphs[index];
    if (!subgraph->enabled) {
      continue;
    }

    const ExecutionPolicy policy = subgraph->execution_policy();
    if (policy.trigger == ExecutionTrigger::OneShot &&
        index < m_one_shot_executed.size() && m_one_shot_executed[index]) {
      continue;
    }

    if (policy.trigger == ExecutionTrigger::OnDemand &&
        !subgraph->consume_process_request()) {
      continue;
    }

    for (size_t dependency_index : m_dependencies[index]) {
      wait_for_subgraph(dependency_index);
    }

    std::unordered_map<std::string, const SubgraphOutputData *> resolved_inputs;
    for (const auto &port : subgraph->input_ports()) {
      auto iterator = available_outputs.find(port.name);
      if (iterator != available_outputs.end()) {
        resolved_inputs[port.name] = iterator->second;
      }
    }

    JobQueue queue = JobQueue::Main;
    switch (policy.queue_hint) {
      case QueueHint::Main:
        queue = JobQueue::Main;
        break;
      case QueueHint::AsyncCompute:
        queue = JobQueue::Worker;
        break;
      case QueueHint::Background:
        queue = JobQueue::Background;
        break;
    }

    if (queue == JobQueue::Main || jobs == nullptr) {
      LOG_DEBUG(
          "[TerrainGraph] processing subgraph on main thread",
          subgraph->name(),
          "trigger=", trigger_name(policy.trigger),
          "queue=", queue_hint_name(policy.queue_hint)
      );
      subgraph->process(resolved_inputs);
      register_outputs(index);
    } else {
      auto *subgraph_ptr = subgraph.get();
      m_running_jobs[index] = jobs->submit(
          [subgraph_ptr, inputs = std::move(resolved_inputs)]() mutable {
            subgraph_ptr->process(inputs);
          },
          queue
      );
      LOG_DEBUG(
          "[TerrainGraph] dispatched async subgraph",
          subgraph->name(),
          "trigger=", trigger_name(policy.trigger),
          "queue=", queue_hint_name(policy.queue_hint),
          "job_id=", m_running_jobs[index].id
      );
    }

    if (policy.trigger == ExecutionTrigger::OneShot &&
        index < m_one_shot_executed.size()) {
      m_one_shot_executed[index] = true;
    }
  }
}

std::span<const Scope<TerrainSubgraph>> TerrainGraph::subgraphs() const {
  return m_subgraphs;
}

const TerrainSubgraph *TerrainGraph::find_subgraph(std::string_view name) const {
  for (const auto &subgraph : m_subgraphs) {
    if (subgraph->name() == name) {
      return subgraph.get();
    }
  }
  return nullptr;
}

} // namespace astralix::terrain
