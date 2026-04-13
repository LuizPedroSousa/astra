#include "material-binding.hpp"

#include <gtest/gtest.h>

namespace astralix::rendering {
namespace {

TEST(MaterialBindingTest, FinalizeMaterialBindingStateMirrorsMissingSpecularSlot) {
  MaterialBindingState state;
  state.diffuse_slot = 3;
  state.specular_slot = -1;

  finalize_material_binding_state(state);

  EXPECT_EQ(state.diffuse_slot, 3);
  EXPECT_EQ(state.specular_slot, 3);
}

TEST(MaterialBindingTest, FinalizeMaterialBindingStateMirrorsMissingDiffuseSlot) {
  MaterialBindingState state;
  state.diffuse_slot = -1;
  state.specular_slot = 5;

  finalize_material_binding_state(state);

  EXPECT_EQ(state.diffuse_slot, 5);
  EXPECT_EQ(state.specular_slot, 5);
}

} // namespace
} // namespace astralix::rendering
