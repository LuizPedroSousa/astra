#include "systems/workspace-shell-system-internal.hpp"

namespace astralix::editor {

void WorkspaceShellSystem::mount_panels_from_snapshot() {
  if (!m_active_snapshot.has_value()) {
    return;
  }

  const auto panel_ids = m_panel_order;
  for (const auto &instance_id : panel_ids) {
    const auto panel_it = m_active_snapshot->panels.find(instance_id);
    if (panel_it == m_active_snapshot->panels.end() || !panel_it->second.open) {
      continue;
    }

    if (m_panels.contains(instance_id)) {
      auto &mounted = m_panels.at(instance_id);
      mounted.spec.open = true;
      mounted.spec.provider_id = panel_it->second.provider_id;
      mounted.spec.title = panel_it->second.title;
      mounted.spec.dock_slot = panel_it->second.dock_slot;
      continue;
    }

    const auto *provider = panel_registry()->find(panel_it->second.provider_id);
    if (provider == nullptr || !provider->factory) {
      continue;
    }

    auto controller = provider->factory();
    if (controller == nullptr) {
      continue;
    }

    if (auto panel_state = m_store->decode_panel_state(panel_it->second.state_blob);
        panel_state.has_value()) {
      controller->load_state(*panel_state);
    }

    m_panels.emplace(
        instance_id,
        MountedPanel{
            .spec =
                PanelInstanceSpec{
                    .instance_id = instance_id,
                    .provider_id = panel_it->second.provider_id,
                    .title = panel_it->second.title,
                    .open = panel_it->second.open,
                    .dock_slot = panel_it->second.dock_slot,
                },
            .controller = std::move(controller),
        }
    );
  }

  destroy_unmounted_panels();
}

void WorkspaceShellSystem::destroy_unmounted_panels() {
  if (!m_active_snapshot.has_value()) {
    for (auto &[instance_id, mounted] : m_panels) {
      if (mounted.controller != nullptr) {
        mounted.controller->unmount();
      }
      reset_mounted_panel_runtime(mounted);
    }
    m_panels.clear();
    return;
  }

  std::unordered_set<std::string> keep;
  for (const auto &[instance_id, panel] : m_active_snapshot->panels) {
    if (panel.open) {
      keep.insert(instance_id);
    }
  }

  for (auto it = m_panels.begin(); it != m_panels.end();) {
    if (!keep.contains(it->first)) {
      if (it->second.controller != nullptr) {
        auto state = m_active_snapshot->panels.find(it->first);
        if (state != m_active_snapshot->panels.end()) {
          auto ctx = m_store->create_context();
          it->second.controller->save_state(ctx);
          state->second.state_blob = m_store->encode_panel_state(ctx);
        }

        it->second.controller->unmount();
      }
      reset_mounted_panel_runtime(it->second);
      it = m_panels.erase(it);
      continue;
    }

    ++it;
  }
}

void WorkspaceShellSystem::reset_mounted_panel_runtime(MountedPanel &mounted) {
  mounted.runtime.reset();
  mounted.content_host_node = ui::k_invalid_node_id;
  mounted.last_render_version.reset();
}

void WorkspaceShellSystem::mount_rendered_panel(
    std::string_view,
    MountedPanel &mounted
) {
  if (mounted.controller == nullptr || m_document == nullptr ||
      mounted.content_host_node == ui::k_invalid_node_id) {
    return;
  }

  mounted.runtime =
      create_scope<ui::im::Runtime>(m_document, mounted.content_host_node);

  mounted.controller->mount(PanelMountContext{
      .runtime = mounted.runtime.get(),
      .default_font_id = m_default_font_id,
      .default_font_size = m_default_font_size,
  });
  render_mounted_panel(mounted);
}

void WorkspaceShellSystem::render_mounted_panel(MountedPanel &mounted) {
  const std::string render_zone_name =
      mounted.spec.title + "::render_mounted_panel";
  ASTRA_PROFILE_DYN(render_zone_name.c_str(), render_zone_name.size());

  if (mounted.controller == nullptr || mounted.runtime == nullptr) {
    return;
  }

  const auto render_version = mounted.controller->render_version();
  if (render_version.has_value() &&
      mounted.last_render_version.has_value() &&
      *mounted.last_render_version == *render_version) {
    return;
  }

  mounted.runtime->render(
#ifdef ASTRA_TRACE
      [&controller = mounted.controller,
       &title = mounted.spec.title](ui::im::Frame &ui) {
        const std::string zone_name = title + "::render";
        ASTRA_PROFILE_DYN(zone_name.c_str(), zone_name.size());
        controller->render(ui);
      }
#else
      [&controller = mounted.controller](ui::im::Frame &ui) {
        controller->render(ui);
      }
#endif
  );

  mounted.last_render_version = render_version;
}

} // namespace astralix::editor
