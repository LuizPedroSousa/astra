#include "args.hpp"

#include <gtest/gtest.h>
#include <sstream>

namespace axgen {
namespace {

struct FakeArgv {
  std::vector<std::string> storage;
  std::vector<char *> ptrs;

  FakeArgv(std::initializer_list<const char *> args) {
    storage.emplace_back("axgen");
    for (const char *arg : args) {
      storage.emplace_back(arg);
    }

    for (auto &value : storage) {
      ptrs.push_back(value.data());
    }
  }

  int argc() const { return static_cast<int>(ptrs.size()); }
  char **argv() { return ptrs.data(); }
};

std::vector<Token> make_tokens(std::initializer_list<const char *> args) {
  FakeArgv fake_argv(args);
  return tokenize(fake_argv.argc(), fake_argv.argv());
}

} // namespace

TEST(AxgenArgs, ParsesSyncShadersCommand) {
  Options options;
  EXPECT_TRUE(parse(make_tokens({"sync-shaders"}), options));
  EXPECT_EQ(options.command, Options::Command::SyncShaders);
}

TEST(AxgenArgs, ParsesCookAssetsCommand) {
  Options options;
  EXPECT_TRUE(parse(make_tokens({"cook-assets"}), options));
  EXPECT_EQ(options.command, Options::Command::CookAssets);
}

TEST(AxgenArgs, ParsesManifestAndWatchFlags) {
  Options options;
  EXPECT_TRUE(parse(make_tokens(
      {"sync-shaders", "--manifest", "examples/sandbox/src/project.ax", "--watch"}),
                    options));
  EXPECT_EQ(options.command, Options::Command::SyncShaders);
  EXPECT_EQ(options.manifest_path, "examples/sandbox/src/project.ax");
  EXPECT_TRUE(options.watch);
}

TEST(AxgenArgs, AllowsHelpWithoutCommand) {
  Options options;
  EXPECT_TRUE(parse(make_tokens({"--help"}), options));
  EXPECT_TRUE(options.help);
}

TEST(AxgenArgs, RejectsUnknownCommand) {
  Options options;
  EXPECT_FALSE(parse(make_tokens({"compile"}), options));
}

TEST(AxgenArgs, RejectsUnexpectedValueAfterCommand) {
  Options options;
  EXPECT_FALSE(parse(make_tokens({"sync-shaders", "extra"}), options));
}

TEST(AxgenArgs, RejectsManifestWithoutValue) {
  Options options;
  EXPECT_FALSE(parse(make_tokens({"sync-shaders", "--manifest"}), options));
}

TEST(AxgenArgs, ParsesRootFlag) {
  Options options;
  EXPECT_TRUE(parse(
      make_tokens({"sync-shaders", "--root", "examples/sandbox"}), options));
  EXPECT_EQ(options.command, Options::Command::SyncShaders);
  EXPECT_EQ(options.root_path, "examples/sandbox");
}

TEST(AxgenArgs, RejectsRootWithoutValue) {
  Options options;
  EXPECT_FALSE(parse(make_tokens({"sync-shaders", "--root"}), options));
}

TEST(AxgenArgs, RejectsWatchForCookAssets) {
  Options options;
  EXPECT_FALSE(parse(make_tokens({"cook-assets", "--watch"}), options));
}

} // namespace axgen
