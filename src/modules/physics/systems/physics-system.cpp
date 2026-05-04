#include "physics-system.hpp"

#include "PxPhysics.h"
#include "trace.hpp"
#include "PxPhysicsAPI.h"
#include "PxSimulationEventCallback.h"
#include "collider-shape-resolution.hpp"
#include "components/collider.hpp"
#include "components/rigidbody.hpp"
#include "components/transform.hpp"
#include "console.hpp"
#include "events/key-codes.hpp"
#include "events/keyboard.hpp"
#include "extensions/PxRigidBodyExt.h"
#include "foundation/PxFoundation.h"
#include "foundation/PxQuat.h"
#include "foundation/PxSimpleTypes.h"
#include "foundation/PxVec3.h"
#include "managers/scene-manager.hpp"
#include "managers/window-manager.hpp"
#include "utils/math.hpp"
#include <algorithm>
#include <vector>

using namespace physx;

static PxDefaultAllocator g_allocator;
static PxDefaultErrorCallback g_error_callback;
static PxFoundation *g_foundation = nullptr;
static PxPhysics *g_physics = nullptr;
static PxDefaultCpuDispatcher *g_dispatcher = nullptr;
static PxScene *g_scene = nullptr;
static PxMaterial *g_material = nullptr;
static PxPvd *g_pvd = nullptr;
static bool g_simulate = true;

namespace astralix {

bool physics_simulation_enabled() { return g_simulate; }

void set_physics_simulation_enabled(bool enabled) { g_simulate = enabled; }

void toggle_physics_simulation() { g_simulate = !g_simulate; }

namespace {

void clear_registered_actors(
    std::unordered_map<EntityID, PxRigidActor *> &actors
) {
  if (g_scene == nullptr) {
    actors.clear();
    return;
  }

  for (auto &[_, actor] : actors) {
    if (actor != nullptr) {
      g_scene->removeActor(*actor);
      actor->release();
    }
  }

  actors.clear();
}

PxTransform to_px_transform(const scene::Transform &transform) {
  return PxTransform(GlmVec3ToPxVec3(transform.position),
                     GlmQuatToPxQuat(transform.rotation));
}

void attach_collider(PxRigidActor *actor, const ResolvedCollider &collider) {
  if (g_physics == nullptr || g_material == nullptr) {
    return;
  }

  const glm::vec3 half_extents =
      glm::max(collider.half_extents, glm::vec3(0.001f));

  PxShape *shape = g_physics->createShape(
      PxBoxGeometry(half_extents.x, half_extents.y, half_extents.z),
      *g_material, true);
  shape->setLocalPose(PxTransform(GlmVec3ToPxVec3(collider.center)));
  actor->attachShape(*shape);
  shape->release();
}

PxRigidActor *create_actor(ecs::EntityRef entity,
                           const scene::Transform &transform,
                           const physics::RigidBody &rigid_body) {
  if (g_physics == nullptr) {
    return nullptr;
  }

  PxRigidActor *actor = nullptr;
  const PxTransform pose = to_px_transform(transform);

  switch (rigid_body.mode) {
    case physics::RigidBodyMode::Dynamic:
      actor = g_physics->createRigidDynamic(pose);
      break;
    case physics::RigidBodyMode::Static:
      actor = g_physics->createRigidStatic(pose);
      break;
  }

  if (actor == nullptr) {
    return nullptr;
  }

  if (auto *collider = entity.get<physics::BoxCollider>();
      collider != nullptr) {
    attach_collider(actor, resolve_collider_shape(*collider));
  }

  if (auto *dynamic_actor = actor->is<PxRigidDynamic>();
      dynamic_actor != nullptr && actor->getNbShapes() > 0) {
    PxRigidBodyExt::updateMassAndInertia(*dynamic_actor,
                                         std::max(rigid_body.mass, 0.001f));
  }

  return actor;
}

void set_actor_pose(PxRigidActor *actor, const PxTransform &pose) {
  if (auto *dynamic_actor = actor->is<PxRigidDynamic>();
      dynamic_actor != nullptr) {
    dynamic_actor->setGlobalPose(pose, true);
    return;
  }

  if (auto *static_actor = actor->is<PxRigidStatic>();
      static_actor != nullptr) {
    static_actor->setGlobalPose(pose);
  }
}

PxTransform actor_pose(PxRigidActor *actor) {
  if (auto *dynamic_actor = actor->is<PxRigidDynamic>();
      dynamic_actor != nullptr) {
    return dynamic_actor->getGlobalPose();
  }

  return static_cast<PxRigidStatic *>(actor)->getGlobalPose();
}

} // namespace

PhysicsSystem::PhysicsSystem(PhysicsSystemConfig &config)
    : m_backend(config.backend), m_gravity(config.gravity),
      m_pvd({.host = config.pvd_host,
             .port = config.pvd_port,
             .timeout = config.pvd_timeout}) {}

void PhysicsSystem::start() {
  ASTRA_PROFILE_N("PhysicsSystem::start");
  g_foundation =
      PxCreateFoundation(PX_PHYSICS_VERSION, g_allocator, g_error_callback);
  ASTRA_ENSURE(g_foundation == nullptr, "[PHYSICS SYSTEM] foundation failed")

  g_pvd = PxCreatePvd(*g_foundation);

  PxPvdTransport *transport = PxDefaultPvdSocketTransportCreate(
      m_pvd.host.c_str(), m_pvd.port, m_pvd.timeout);
  if (g_pvd != nullptr && transport != nullptr) {
    g_pvd->connect(*transport, PxPvdInstrumentationFlag::eALL);
  }

  g_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *g_foundation,
                              PxTolerancesScale(), true, g_pvd);
  ASTRA_ENSURE(g_physics == nullptr, "[PHYSICS SYSTEM] physics init failed")

  PxSceneDesc scene_desc(g_physics->getTolerancesScale());
  scene_desc.gravity = PxVec3(m_gravity.x, m_gravity.y, m_gravity.z);
  g_dispatcher = PxDefaultCpuDispatcherCreate(2);
  scene_desc.cpuDispatcher = g_dispatcher;
  scene_desc.filterShader = PxDefaultSimulationFilterShader;

  g_scene = g_physics->createScene(scene_desc);
  ASTRA_ENSURE(g_scene == nullptr, "[PHYSICS SYSTEM] scene init failed")

  g_material = g_physics->createMaterial(0.5f, 0.5f, 0.6f);

  if (auto *pvd_client = g_scene->getScenePvdClient(); pvd_client != nullptr) {
    pvd_client->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
    pvd_client->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
    pvd_client->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
  }
}

void PhysicsSystem::fixed_update(double fixed_dt) {
  ASTRA_PROFILE_N("PhysicsSystem::fixed_update");
  if (g_scene == nullptr) {
    return;
  }

  auto scene_manager = SceneManager::get();
  if (scene_manager == nullptr) {
    clear_registered_actors(m_actors);
    m_registered_scene = nullptr;
    m_registered_scene_revision = 0u;
    m_registered_scene_generation = 0u;
    return;
  }

  (void)scene_manager->flush_pending_active_scene_state();

  const uint64_t scene_generation =
      scene_manager->scene_instance_generation();
  if (m_registered_scene_generation != scene_generation) {
    clear_registered_actors(m_actors);
    m_registered_scene = nullptr;
    m_registered_scene_revision = 0u;
    m_registered_scene_generation = scene_generation;
  }

  auto active_scene = scene_manager->get_active_scene();
  if (active_scene == nullptr) {
    clear_registered_actors(m_actors);
    m_registered_scene = nullptr;
    m_registered_scene_revision = 0u;
    m_registered_scene_generation = scene_generation;
    return;
  }

  if (active_scene->get_session_kind() != SceneSessionKind::Preview &&
      active_scene->get_session_kind() != SceneSessionKind::Runtime) {
    clear_registered_actors(m_actors);
    m_registered_scene = nullptr;
    m_registered_scene_revision = 0u;
    m_registered_scene_generation = scene_generation;
    return;
  }

  if (m_registered_scene != active_scene ||
      m_registered_scene_revision != active_scene->get_session_revision()) {
    // Preview and runtime worlds can reuse the same entity ids, so actors
    // must be rebuilt whenever the active scene instance changes or is reset.
    clear_registered_actors(m_actors);
    m_registered_scene = active_scene;
    m_registered_scene_revision = active_scene->get_session_revision();
  }

  auto &world = active_scene->world();
  resolve_box_colliders_from_render_mesh(world);
  std::vector<EntityID> stale_entities;
  stale_entities.reserve(m_actors.size());

  for (const auto &[entity_id, actor] : m_actors) {
    if (!world.contains(entity_id) || !world.active(entity_id) ||
        !world.has<scene::Transform>(entity_id) ||
        !world.has<physics::RigidBody>(entity_id)) {
      if (actor != nullptr) {
        g_scene->removeActor(*actor);
        actor->release();
      }
      stale_entities.push_back(entity_id);
    }
  }

  for (EntityID entity_id : stale_entities) {
    m_actors.erase(entity_id);
  }

  world.each<scene::Transform, physics::RigidBody>(
      [&](EntityID entity_id, scene::Transform &transform,
          physics::RigidBody &rigid_body) {
        if (!world.active(entity_id) || m_actors.contains(entity_id)) {
          return;
        }

        auto entity = world.entity(entity_id);
        auto *actor = create_actor(entity, transform, rigid_body);
        if (actor == nullptr) {
          return;
        }

        g_scene->addActor(*actor);
        m_actors.emplace(entity_id, actor);
      });

  for (auto &[entity_id, actor] : m_actors) {
    auto entity = world.entity(entity_id);
    auto *transform = entity.get<scene::Transform>();
    if (actor == nullptr || transform == nullptr || !transform->dirty) {
      continue;
    }

    set_actor_pose(actor, to_px_transform(*transform));
  }

  if (!g_simulate || !active_scene->is_playing() || m_actors.empty()) {
    return;
  }

  {
    ASTRA_PROFILE_N("PhysX::simulate");
    g_scene->simulate(fixed_dt);
    g_scene->fetchResults(true);
  }

  for (auto &[entity_id, actor] : m_actors) {
    auto entity = world.entity(entity_id);
    auto *transform = entity.get<scene::Transform>();
    if (actor == nullptr || transform == nullptr) {
      continue;
    }

    const PxTransform pose = actor_pose(actor);
    transform->position = glm::vec3(pose.p.x, pose.p.y, pose.p.z);
    transform->rotation = PxQuatToGlmQuat(pose.q);
    transform->dirty = true;
  }
}

void PhysicsSystem::pre_update(double dt) {}

void PhysicsSystem::update(double dt) {
  if (ConsoleManager::get().captures_input()) {
    return;
  }

  if (input::IS_KEY_RELEASED(input::KeyCode::F6)) {
    toggle_physics_simulation();
  }
}

PhysicsSystem::~PhysicsSystem() {
  for (auto &[_, actor] : m_actors) {
    if (actor != nullptr) {
      if (g_scene != nullptr) {
        g_scene->removeActor(*actor);
      }
      actor->release();
    }
  }
  m_actors.clear();

  if (g_scene != nullptr) {
    g_scene->release();
    g_scene = nullptr;
  }

  if (g_dispatcher != nullptr) {
    g_dispatcher->release();
    g_dispatcher = nullptr;
  }

  if (g_material != nullptr) {
    g_material->release();
    g_material = nullptr;
  }

  if (g_physics != nullptr) {
    g_physics->release();
    g_physics = nullptr;
  }

  if (g_pvd != nullptr) {
    auto *transport = g_pvd->getTransport();
    g_pvd->release();
    g_pvd = nullptr;
    if (transport != nullptr) {
      transport->release();
    }
  }

  if (g_foundation != nullptr) {
    g_foundation->release();
    g_foundation = nullptr;
  }

  m_registered_scene = nullptr;
  m_registered_scene_generation = 0u;
}
} // namespace astralix
