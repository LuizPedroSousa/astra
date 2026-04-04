#pragma once
#include "cstddef"
#include "either.hpp"
#include "exceptions/base-exception.hpp"
#include "guid.hpp"
#include "limits"
#include "stdint.h"

namespace astralix {
template <class T>
class System;

using SystemPriority = uint16_t;

static const SystemPriority LOWEST_SYSTEM_PRIORITY =
    std::numeric_limits<SystemPriority>::min();
static const SystemPriority VERY_LOW_SYSTEM_PRIORITY = 99;
static const SystemPriority LOW_SYSTEM_PRIORITY = 100;
static const SystemPriority NORMAL_SYSTEM_PRIORITY = 200;
static const SystemPriority MEDIUM_SYSTEM_PRIORITY = 300;
static const SystemPriority HIGH_SYSTEM_PRIORITY = 400;
static const SystemPriority VERY_HIGH_SYSTEM_PRIORITY = 401;
static const SystemPriority HIGHEST_SYSTEM_PRIORITY =
    std::numeric_limits<SystemPriority>::max();

class ISystem {
  friend class SystemManager;

protected:
  explicit ISystem(SystemPriority p_priority = NORMAL_SYSTEM_PRIORITY, double update_interval_ms = (-1.0));

public:
  virtual ~ISystem();

  virtual inline SystemTypeID get_system_type_id() const = 0;
  virtual inline const char *get_system_type_name() const = 0;

  virtual void start() = 0;
  virtual void fixed_update(double fixed_dt) = 0;
  virtual void pre_update(double dt) = 0;
  virtual void update(double dt) = 0;
  bool is_active() { return m_is_active; };
  void set_active(bool is_active) { m_is_active = is_active; };

  // virtual void post_update(double dt) = 0;
  //
  //
private:
  double m_time_since_last_update{0};

  SystemPriority m_priority;

  double m_updater_internal;

  uint8_t m_enabled : 1;
  uint8_t m_needs_update : 1;
  uint8_t m_reserved : 6;
  bool m_is_active = true;
};

} // namespace astralix
