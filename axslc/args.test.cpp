#include "args.hpp"
#include <gtest/gtest.h>
#include <sstream>

struct FakeArgv {
  std::vector<std::string> storage;
  std::vector<char *> ptrs;

  FakeArgv(std::initializer_list<const char *> args) {
    storage.emplace_back("axslc");
    for (const char *a : args)
      storage.emplace_back(a);
    for (auto &s : storage)
      ptrs.push_back(s.data());
  }

  int argc() const { return static_cast<int>(ptrs.size()); }
  char **argv() { return ptrs.data(); }
};

TEST(Tokenize, EmptyArgv) {
  FakeArgv fake_argument{};
  auto tokens = tokenize(fake_argument.argc(), fake_argument.argv());
  EXPECT_TRUE(tokens.empty());
}

TEST(Tokenize, SingleValue) {
  FakeArgv fake_argument{"shader.axsl"};
  auto tokens = tokenize(fake_argument.argc(), fake_argument.argv());
  ASSERT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, Token::Kind::Value);
  EXPECT_EQ(tokens[0].text, "shader.axsl");
}

TEST(Tokenize, SingleFlag) {
  FakeArgv fake_argument{"--verbose"};
  auto tokens = tokenize(fake_argument.argc(), fake_argument.argv());
  ASSERT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, Token::Kind::Flag);
  EXPECT_EQ(tokens[0].text, "--verbose");
}

TEST(Tokenize, ShortFlag) {
  FakeArgv fake_argument{"-v"};
  auto tokens = tokenize(fake_argument.argc(), fake_argument.argv());
  ASSERT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, Token::Kind::Flag);
}

TEST(Tokenize, MixedFlagsAndValues) {
  FakeArgv fake_argument{"-v", "-o", "build/", "shader.axsl"};
  auto tokens = tokenize(fake_argument.argc(), fake_argument.argv());
  ASSERT_EQ(tokens.size(), 4u);
  EXPECT_EQ(tokens[0].kind, Token::Kind::Flag);
  EXPECT_EQ(tokens[1].kind, Token::Kind::Flag);
  EXPECT_EQ(tokens[2].kind, Token::Kind::Value);
  EXPECT_EQ(tokens[3].kind, Token::Kind::Value);
}

TEST(Tokenize, SingleDashIsValue) {
  FakeArgv fake_argument{"-"};
  auto tokens = tokenize(fake_argument.argc(), fake_argument.argv());
  ASSERT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, Token::Kind::Value);
}

static std::vector<Token>
make_tokens(std::initializer_list<const char *> args) {
  FakeArgv fake_argument(args);
  return tokenize(fake_argument.argc(), fake_argument.argv());
}

TEST(Parse, HelpShort) {
  Options opts;
  EXPECT_TRUE(parse(make_tokens({"-h"}), opts));
  EXPECT_TRUE(opts.help);
}

TEST(Parse, HelpLong) {
  Options opts;
  EXPECT_TRUE(parse(make_tokens({"--help"}), opts));
  EXPECT_TRUE(opts.help);
}

TEST(Parse, Version) {
  Options opts;
  EXPECT_TRUE(parse(make_tokens({"--version"}), opts));
  EXPECT_TRUE(opts.version);
}

TEST(Parse, VerboseShort) {
  Options opts;
  EXPECT_TRUE(parse(make_tokens({"-v", "shader.axsl"}), opts));
  EXPECT_TRUE(opts.verbose);
  EXPECT_EQ(opts.input_file, "shader.axsl");
}

TEST(Parse, VerboseLong) {
  Options opts;
  EXPECT_TRUE(parse(make_tokens({"--verbose", "shader.axsl"}), opts));
  EXPECT_TRUE(opts.verbose);
}

TEST(Parse, OutputShort) {
  Options opts;
  EXPECT_TRUE(parse(make_tokens({"-o", "build/", "shader.axsl"}), opts));
  EXPECT_EQ(opts.output_dir, "build/");
  EXPECT_EQ(opts.input_file, "shader.axsl");
}

TEST(Parse, OutputLong) {
  Options opts;
  EXPECT_TRUE(parse(make_tokens({"--output", "out/", "shader.axsl"}), opts));
  EXPECT_EQ(opts.output_dir, "out/");
}

TEST(Parse, IncludePathShort) {
  Options opts;
  EXPECT_TRUE(parse(make_tokens({"-I", "./shaders", "shader.axsl"}), opts));
  EXPECT_EQ(opts.include_path, "./shaders");
}

TEST(Parse, IncludePathLong) {
  Options opts;
  EXPECT_TRUE(
      parse(make_tokens({"--include-path", "./inc", "shader.axsl"}), opts));
  EXPECT_EQ(opts.include_path, "./inc");
}

TEST(Parse, AllOptionsTogether) {
  Options opts;
  EXPECT_TRUE(parse(
      make_tokens({"-v", "-o", "out/", "-I", "inc/", "shader.axsl"}), opts));
  EXPECT_TRUE(opts.verbose);
  EXPECT_EQ(opts.output_dir, "out/");
  EXPECT_EQ(opts.include_path, "inc/");
  EXPECT_EQ(opts.input_file, "shader.axsl");
}

TEST(Parse, InputFileBeforeFlags) {
  Options opts;
  EXPECT_TRUE(parse(make_tokens({"shader.axsl", "-v"}), opts));
  EXPECT_EQ(opts.input_file, "shader.axsl");
  EXPECT_TRUE(opts.verbose);
}

class ParseError : public testing::Test {
protected:
  void SetUp() override { old_buf_ = std::cerr.rdbuf(sink_.rdbuf()); }
  void TearDown() override { std::cerr.rdbuf(old_buf_); }

private:
  std::ostringstream sink_;
  std::streambuf *old_buf_;
};

TEST_F(ParseError, NoInputFile) {
  Options opts;
  EXPECT_FALSE(parse(make_tokens({}), opts));
}

TEST_F(ParseError, UnknownFlag) {
  Options opts;
  EXPECT_FALSE(parse(make_tokens({"--unknown"}), opts));
}

TEST_F(ParseError, MultipleInputFiles) {
  Options opts;
  EXPECT_FALSE(parse(make_tokens({"a.axsl", "b.axsl"}), opts));
}

TEST_F(ParseError, OutputMissingArgument) {
  Options opts;
  EXPECT_FALSE(parse(make_tokens({"-o"}), opts));
}

TEST_F(ParseError, OutputFollowedByFlag) {
  Options opts;
  EXPECT_FALSE(parse(make_tokens({"-o", "--verbose", "shader.axsl"}), opts));
}

TEST_F(ParseError, IncludePathMissingArgument) {
  Options opts;
  EXPECT_FALSE(parse(make_tokens({"-I"}), opts));
}

TEST(Parse, DefaultsAreClean) {
  Options opts;
  parse(make_tokens({"shader.axsl"}), opts);
  EXPECT_FALSE(opts.verbose);
  EXPECT_FALSE(opts.help);
  EXPECT_FALSE(opts.version);
  EXPECT_TRUE(opts.output_dir.empty());
  EXPECT_TRUE(opts.include_path.empty());
}
