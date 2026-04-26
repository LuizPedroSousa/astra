#include "audio-graph.hpp"
#include "assert.hpp"
#include "trace.hpp"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace astralix::audio {

void AudioGraph::add_pass(Scope<AudioPass> pass) {
  m_passes.push_back(std::move(pass));
  m_compiled = false;
}

void AudioGraph::compile() {
  ASTRA_PROFILE_N("AudioGraph::compile");

  size_t pass_count = m_passes.size();

  std::unordered_map<FrameField, std::vector<size_t>> writers;
  for (size_t index = 0; index < pass_count; ++index) {
    for (FrameField field : m_passes[index]->writes()) {
      writers[field].push_back(index);
    }
  }

  std::vector<std::unordered_set<size_t>> dependencies(pass_count);
  for (size_t index = 0; index < pass_count; ++index) {
    for (FrameField field : m_passes[index]->reads()) {
      auto iterator = writers.find(field);
      if (iterator == writers.end()) {
        continue;
      }
      for (size_t writer_index : iterator->second) {
        if (writer_index != index) {
          dependencies[index].insert(writer_index);
        }
      }
    }
  }

  std::vector<size_t> sorted_order;
  sorted_order.reserve(pass_count);
  std::vector<bool> visited(pass_count, false);
  std::vector<bool> in_stack(pass_count, false);

  auto topological_visit = [&](auto &self, size_t node) -> void {
    if (visited[node]) {
      return;
    }

    ASTRA_ENSURE(in_stack[node], "AudioGraph: cyclic dependency detected involving pass '",
                  m_passes[node]->name(), "'");

    in_stack[node] = true;

    for (size_t dependency : dependencies[node]) {
      self(self, dependency);
    }

    in_stack[node] = false;
    visited[node] = true;
    sorted_order.push_back(node);
  };

  for (size_t index = 0; index < pass_count; ++index) {
    topological_visit(topological_visit, index);
  }

  std::vector<Scope<AudioPass>> sorted_passes;
  sorted_passes.reserve(pass_count);
  for (size_t index : sorted_order) {
    sorted_passes.push_back(std::move(m_passes[index]));
  }
  m_passes = std::move(sorted_passes);
  m_compiled = true;
}

void AudioGraph::setup(AudioFrame &frame, AudioBackend &backend) {
  ASTRA_ENSURE(!m_compiled, "AudioGraph::setup called before compile()");

  for (auto &pass : m_passes) {
    if (pass->enabled) {
      pass->setup(frame, backend);
    }
  }
}

void AudioGraph::process(AudioFrame &frame, AudioBackend &backend) {
  ASTRA_PROFILE_N("AudioGraph::process");

  for (auto &pass : m_passes) {
    if (pass->enabled) {
      pass->process(frame, backend);
    }
  }
}

void AudioGraph::teardown(AudioFrame &frame, AudioBackend &backend) {
  for (auto it = m_passes.rbegin(); it != m_passes.rend(); ++it) {
    (*it)->teardown(frame, backend);
  }
}

std::span<const Scope<AudioPass>> AudioGraph::passes() const {
  return m_passes;
}

} // namespace astralix::audio
