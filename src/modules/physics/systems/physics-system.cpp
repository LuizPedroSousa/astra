#include "physics-system.hpp"
#include "PxShape.h"
#include "PxSimulationEventCallback.h"
#include "assert.hpp"
#include "components/rigidbody/rigidbody-component.hpp"
#include "components/transform/transform-component.hpp"
#include "events/key-codes.hpp"
#include "events/keyboard.hpp"
#include "managers/entity-manager.hpp"

#include "entities/object.hpp"

#include "PxPhysics.h"
#include "PxPhysicsAPI.h"
#include "foundation/PxFoundation.h"

#include "foundation/PxQuat.h"
#include "foundation/PxSimpleTypes.h"
#include "foundation/PxVec3.h"

#include <ctype.h>

#include "foundation/PxPreprocessor.h"
#include "managers/window-manager.hpp"
#include "project.hpp"
#include "utils/math.hpp"

using namespace physx;

static PxDefaultAllocator gAllocator;
static PxDefaultErrorCallback gErrorCallback;
static PxFoundation *gFoundation = NULL;
static PxPhysics *gPhysics = NULL;
static PxDefaultCpuDispatcher *gDispatcher = NULL;
static PxScene *gScene = NULL;
static PxMaterial *gMaterial = NULL;
static PxPvd *gPvd = NULL;
static bool gSimulate = false;

namespace astralix {

PhysicsSystem::PhysicsSystem(PhysicsSystemConfig &config)
    : m_backend(config.backend), m_gravity(config.gravity),
      m_pvd({.host = config.pvd_host,
             .port = config.pvd_port,
             .timeout = config.pvd_timeout}) {};

void PhysicsSystem::start() {
  auto entity_manager = EntityManager::get();

  gFoundation =
      PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);

  ASTRA_ENSURE(!gFoundation, "[PHYSICS SYSTEM] foundation failed!")

  gPvd = PxCreatePvd(*gFoundation);

  PxPvdTransport *transport = PxDefaultPvdSocketTransportCreate(
      m_pvd.host.c_str(), m_pvd.port, m_pvd.timeout);
  gPvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

  gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation,
                             PxTolerancesScale(), true, gPvd);

  PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
  sceneDesc.gravity = PxVec3(m_gravity.x, m_gravity.y, m_gravity.z);

  gDispatcher = PxDefaultCpuDispatcherCreate(2);

  sceneDesc.cpuDispatcher = gDispatcher;
  sceneDesc.filterShader = PxDefaultSimulationFilterShader;

  gScene = gPhysics->createScene(sceneDesc);

  PxPvdSceneClient *pvdClient = gScene->getScenePvdClient();

  if (pvdClient) {
    pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
    pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
    pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
  }

  entity_manager->for_each<Object>([](Object *object) {
    auto rigid_body_component = object->get_component<RigidBodyComponent>();

    if (rigid_body_component != nullptr) {
      rigid_body_component->start(gPhysics, gScene);
    }
  });
}

void PhysicsSystem::fixed_update(double fixed_dt) {

  if (!gSimulate)
    return;

  PxU32 actorCount = gScene->getNbActors(PxActorTypeFlag::eRIGID_STATIC |
                                         PxActorTypeFlag::eRIGID_DYNAMIC);

  std::vector<PxActor *> actors(actorCount);
  gScene->getActors(PxActorTypeFlag::eRIGID_STATIC |
                        PxActorTypeFlag::eRIGID_DYNAMIC,
                    actors.data(), actorCount);

  for (PxActor *actor : actors) {
    if (!actor->userData) {
      continue;
    }

    IEntity *entity = static_cast<IEntity *>(actor->userData);
    auto transform = entity->get_component<TransformComponent>();

    if (!transform) {
      continue;
    }

    // Check if transform was modified (m_dirty flag)
    if (transform->m_dirty) {
      // Apply the transform to the PhysX actor
      physx::PxTransform newPose(GlmVec3ToPxVec3(transform->position),
                                 GlmQuatToPxQuat(transform->rotation));

      if (actor->is<PxRigidDynamic>()) {
        PxRigidDynamic *dynamic = static_cast<PxRigidDynamic *>(actor);
        dynamic->setGlobalPose(newPose, true); // True to wake the actor
      } else if (actor->is<PxRigidStatic>()) {
        PxRigidStatic *staticActor = static_cast<PxRigidStatic *>(actor);
        staticActor->setGlobalPose(newPose);
      }

      // Reset dirty flag after applying changes
      transform->m_dirty = false;
      transform->recalculate_transform();
    }
  }

  gScene->simulate(fixed_dt);
  gScene->fetchResults(true);

  for (PxActor *actor : actors) {
    if (actor->userData) {
      IEntity *entity = static_cast<IEntity *>(actor->userData);

      auto transform = entity->get_component<TransformComponent>();

      if (!transform)
        continue;
      PxTransform pose;
      if (actor->is<PxRigidDynamic>()) {
        pose = static_cast<PxRigidDynamic *>(actor)->getGlobalPose();
      } else {
        pose = static_cast<PxRigidStatic *>(actor)->getGlobalPose();
      }

      glm::vec3 physicsPos(pose.p.x, pose.p.y, pose.p.z);
      glm::quat physicsQuat(pose.q.w, pose.q.x, pose.q.y, pose.q.z);

      // Set absolute position and rotation
      transform->translate(physicsPos);
      transform->rotate(physicsQuat);

      transform->recalculate_transform(); // Ensure matrix is updated
    }
  }
};

void PhysicsSystem::pre_update(double dt) {

};

void PhysicsSystem::update(double dt) {
  if (IS_KEY_RELEASED(input::KeyCode::F4)) {
    gSimulate = !gSimulate;
  }

  auto component_manager = ComponentManager::get();
  auto entity_manager = EntityManager::get();

  entity_manager->for_each<Object>([dt](Object *object) {
    auto rigidbody_component = object->get_component<RigidBodyComponent>();

    if (rigidbody_component != nullptr) {
      rigidbody_component->update(dt);
    }
  });
};

PhysicsSystem::~PhysicsSystem() {
  gPhysics->release();
  gFoundation->release();
};
} // namespace astralix
