#include "systems/render-system/core/pass-recorder.hpp"

#include "exceptions/base-exception.hpp"
#include "systems/render-system/passes/render-graph-pass.hpp"
#include <gtest/gtest.h>

namespace astralix {

namespace {

class DummyFramePass : public FramePass {
public:
  std::string name() const override { return "dummy-frame-pass"; }

  void setup(PassSetupContext &ctx) override {
    setup_called = true;
    setup_resource_count = ctx.resource_views().size();
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    record_called = true;
    last_dt = ctx.dt;
    guard_was_active = RenderApiAccessScope::active();

    RenderingInfo info;
    info.debug_name = "dummy-recording";
    info.extent = ImageExtent{.width = 128, .height = 64, .depth = 1};

    recorder.begin_rendering(info);
    recorder.bind_pipeline(RenderPipelineHandle{7});
    recorder.draw_indexed(DrawIndexedArgs{.index_count = 6});
    recorder.end_rendering();
  }

  bool setup_called = false;
  bool record_called = false;
  size_t setup_resource_count = 0;
  double last_dt = 0.0;
  bool guard_was_active = false;
};

TEST(PassRecorderTest, EmitsCommandsInAuthoringOrder) {
  PassRecorder recorder;

  RenderingInfo info;
  info.debug_name = "main";
  info.extent = ImageExtent{.width = 32, .height = 32, .depth = 1};

  recorder.begin_rendering(info);
  recorder.bind_pipeline(RenderPipelineHandle{3});
  recorder.bind_vertex_buffer(BufferHandle{4}, 1, 16);
  recorder.bind_index_buffer(BufferHandle{5}, IndexType::Uint32, 8);
  recorder.draw_indexed(DrawIndexedArgs{.index_count = 12, .instance_count = 2});
  recorder.end_rendering();

  const auto commands = recorder.take_commands();
  ASSERT_EQ(commands.size(), 6u);
  EXPECT_TRUE(std::holds_alternative<BeginRenderingCmd>(commands[0]));
  EXPECT_TRUE(std::holds_alternative<BindPipelineCmd>(commands[1]));
  EXPECT_TRUE(std::holds_alternative<BindVertexBufferCmd>(commands[2]));
  EXPECT_TRUE(std::holds_alternative<BindIndexBufferCmd>(commands[3]));
  EXPECT_TRUE(std::holds_alternative<DrawIndexedCmd>(commands[4]));
  EXPECT_TRUE(std::holds_alternative<EndRenderingCmd>(commands[5]));
}

TEST(PassRecorderTest, RejectsUnbalancedRenderingScopes) {
  PassRecorder recorder;

  RenderingInfo info;
  info.extent = ImageExtent{.width = 1, .height = 1, .depth = 1};

  recorder.begin_rendering(info);
  EXPECT_THROW({ recorder.take_commands(); }, BaseException);
}

TEST(RenderGraphPassTest, RecordedPassAppendsCompiledPassToFrame) {
  auto frame_pass = create_scope<DummyFramePass>();
  auto *frame_pass_ptr = frame_pass.get();
  RenderGraphPass graph_pass(std::move(frame_pass));

  graph_pass.add_computed_dependency(11);
  graph_pass.setup(nullptr, {});

  CompiledFrame frame;
  graph_pass.record(42.5, frame, {});

  ASSERT_TRUE(frame_pass_ptr->setup_called);
  EXPECT_EQ(frame_pass_ptr->setup_resource_count, 0u);
  ASSERT_TRUE(frame_pass_ptr->record_called);
  EXPECT_DOUBLE_EQ(frame_pass_ptr->last_dt, 42.5);
  EXPECT_TRUE(frame_pass_ptr->guard_was_active);

  ASSERT_EQ(frame.passes.size(), 1u);
  EXPECT_EQ(frame.passes[0].debug_name, "dummy-frame-pass");
  ASSERT_EQ(frame.passes[0].dependency_pass_indices.size(), 1u);
  EXPECT_EQ(frame.passes[0].dependency_pass_indices[0], 11u);
  ASSERT_EQ(frame.passes[0].commands.size(), 4u);
  EXPECT_TRUE(std::holds_alternative<BeginRenderingCmd>(frame.passes[0].commands[0]));
  EXPECT_TRUE(std::holds_alternative<BindPipelineCmd>(frame.passes[0].commands[1]));
  EXPECT_TRUE(std::holds_alternative<DrawIndexedCmd>(frame.passes[0].commands[2]));
  EXPECT_TRUE(std::holds_alternative<EndRenderingCmd>(frame.passes[0].commands[3]));
}

} // namespace

} // namespace astralix
