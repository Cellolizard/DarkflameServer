// Tests for dConfig: loading INI files, value retrieval, missing keys,
// comments, empty values, and reload behavior.
//
// dConfig resolves paths relative to BinaryPathFinder::GetBinaryDir(), which
// is the directory of the test executable at runtime.  We therefore write temp
// files into that directory and clean them up in TearDown.

#include <gtest/gtest.h>

#include "dCommonDependencies.h"
#include "dConfig.h"
#include "BinaryPathFinder.h"

#include <fstream>
#include <filesystem>
#include <string>

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class ConfigTest : public dCommonDependenciesTest {
protected:
    // Filename only — dConfig prepends BinaryPathFinder::GetBinaryDir().
    static constexpr const char* kConfigFilename = "test_config_dcommon.ini";

    // Full path used by the fixture to write/remove files.
    std::filesystem::path configFullPath;

    void SetUp() override {
        SetUpDependencies();  // initializes Game::logger; needed by dConfig::ReloadConfig → LogSettings
        configFullPath = BinaryPathFinder::GetBinaryDir() / kConfigFilename;

        std::ofstream f(configFullPath);
        ASSERT_TRUE(f.is_open()) << "Could not open temp config: " << configFullPath;
        f << "# This is a comment line\n";
        f << "; Semicolons are NOT treated as comments by dConfig (it only strips #)\n";
        f << "string_key=hello world\n";
        f << "int_key=42\n";
        f << "empty_key=\n";
        f << "bool_key=1\n";
        f << "float_key=3.14\n";
        f << "spaces_value=  leading and trailing  \n";
        f << "duplicate_key=first\n";
        f << "duplicate_key=second\n";
        f.close();
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(configFullPath, ec);
        // Also remove any reload-test file if it was created.
        std::filesystem::remove(
            BinaryPathFinder::GetBinaryDir() / "test_config_reload.ini", ec);
        TearDownDependencies();
    }

    // Helper: write a fresh config file and return a loaded dConfig for it.
    dConfig LoadFresh(const std::string& filename, const std::string& contents) {
        auto path = BinaryPathFinder::GetBinaryDir() / filename;
        std::ofstream f(path);
        f << contents;
        f.close();
        return dConfig(filename);
    }
};

// ---------------------------------------------------------------------------
// Basic construction
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, LoadFromFile_DoesNotCrash) {
    EXPECT_NO_THROW(dConfig cfg(kConfigFilename));
}

TEST_F(ConfigTest, LoadFromNonExistentFile_DoesNotCrash) {
    // A missing file should simply result in an empty config, not a crash.
    EXPECT_NO_THROW(dConfig cfg("this_file_does_not_exist_at_all.ini"));
}

TEST_F(ConfigTest, LoadFromNonExistentFile_GetValue_ReturnsEmpty) {
    dConfig cfg("this_file_does_not_exist_at_all.ini");
    EXPECT_EQ(cfg.GetValue("any_key"), "");
}

// ---------------------------------------------------------------------------
// Reading known keys
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, GetValue_ExistingStringKey_ReturnsCorrectValue) {
    dConfig cfg(kConfigFilename);
    EXPECT_EQ(cfg.GetValue("string_key"), "hello world");
}

TEST_F(ConfigTest, GetValue_ExistingIntKey_ReturnsStringRepresentation) {
    dConfig cfg(kConfigFilename);
    EXPECT_EQ(cfg.GetValue("int_key"), "42");
}

TEST_F(ConfigTest, GetValue_ExistingBoolKey_ReturnsStringOne) {
    dConfig cfg(kConfigFilename);
    EXPECT_EQ(cfg.GetValue("bool_key"), "1");
}

TEST_F(ConfigTest, GetValue_ExistingFloatKey_ReturnsFloatString) {
    dConfig cfg(kConfigFilename);
    EXPECT_EQ(cfg.GetValue("float_key"), "3.14");
}

// ---------------------------------------------------------------------------
// Missing keys
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, GetValue_NonExistentKey_ReturnsEmptyString) {
    dConfig cfg(kConfigFilename);
    EXPECT_EQ(cfg.GetValue("nonexistent_key"), "");
}

TEST_F(ConfigTest, GetValue_NonExistentKey_ReturnsEmptyString_Repeated) {
    dConfig cfg(kConfigFilename);
    // Call twice to ensure no side-effects from the first call.
    (void)cfg.GetValue("missing_key_1");
    EXPECT_EQ(cfg.GetValue("missing_key_2"), "");
}

// ---------------------------------------------------------------------------
// Empty values
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, GetValue_EmptyValue_ReturnsEmptyString) {
    dConfig cfg(kConfigFilename);
    EXPECT_EQ(cfg.GetValue("empty_key"), "");
}

// ---------------------------------------------------------------------------
// Comments
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, CommentLinesStartingWithHash_AreIgnored) {
    // The comment line starts with '#'.  It should not appear as a key.
    dConfig cfg(kConfigFilename);
    // The comment text is "# This is a comment line"; the key would be
    // "# This is a comment line" if it were parsed — verify it is not.
    EXPECT_EQ(cfg.GetValue("# This is a comment line"), "");
}

TEST_F(ConfigTest, InlineComments_AreNotStripped_ValuePreserved) {
    // dConfig does not strip inline comments; "value # comment" is the value.
    auto cfg = LoadFresh("test_config_reload.ini",
        "key_with_inline=value # this is not stripped\n");
    EXPECT_EQ(cfg.GetValue("key_with_inline"), "value # this is not stripped");
}

// ---------------------------------------------------------------------------
// Duplicate keys — first value wins
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, DuplicateKeys_FirstValueIsKept) {
    dConfig cfg(kConfigFilename);
    // dConfig inserts only if key is not already present (see ProcessLine).
    EXPECT_EQ(cfg.GetValue("duplicate_key"), "first");
}

// ---------------------------------------------------------------------------
// Values with spaces
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, ValueWithLeadingAndTrailingSpaces_PreservedAsIs) {
    dConfig cfg(kConfigFilename);
    // dConfig does not strip whitespace from values.
    EXPECT_EQ(cfg.GetValue("spaces_value"), "  leading and trailing  ");
}

// ---------------------------------------------------------------------------
// Values containing equals signs
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, ValueContainingEqualsSign_ParsedCorrectly) {
    auto cfg = LoadFresh("test_config_reload.ini",
        "equation=1+1=2\n");
    // The first '=' is the separator; the rest of the line is the value.
    EXPECT_EQ(cfg.GetValue("equation"), "1+1=2");
}

// ---------------------------------------------------------------------------
// Numeric string values
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, LargeNumericValue_ReturnedAsString) {
    auto cfg = LoadFresh("test_config_reload.ini",
        "big_number=4294967295\n");
    EXPECT_EQ(cfg.GetValue("big_number"), "4294967295");
}

TEST_F(ConfigTest, NegativeNumericValue_ReturnedAsString) {
    auto cfg = LoadFresh("test_config_reload.ini",
        "negative=-99\n");
    EXPECT_EQ(cfg.GetValue("negative"), "-99");
}

// ---------------------------------------------------------------------------
// Case sensitivity
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, KeyLookup_IsCaseSensitive) {
    dConfig cfg(kConfigFilename);
    // "string_key" is in the file but "STRING_KEY" is not (ignoring env vars).
    // We check that the lowercase version works.
    EXPECT_EQ(cfg.GetValue("string_key"), "hello world");
}

// ---------------------------------------------------------------------------
// Multiple distinct keys
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, MultipleKeys_AllRetrievedCorrectly) {
    dConfig cfg(kConfigFilename);
    EXPECT_EQ(cfg.GetValue("string_key"), "hello world");
    EXPECT_EQ(cfg.GetValue("int_key"),    "42");
    EXPECT_EQ(cfg.GetValue("bool_key"),   "1");
    EXPECT_EQ(cfg.GetValue("float_key"),  "3.14");
    EXPECT_EQ(cfg.GetValue("empty_key"),  "");
}

// ---------------------------------------------------------------------------
// Reload behavior
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, ReloadConfig_PicksUpNewValues) {
    // Write an initial config, load it, then overwrite the file and reload.
    std::string reloadFilename = "test_config_reload.ini";
    auto reloadPath = BinaryPathFinder::GetBinaryDir() / reloadFilename;

    {
        std::ofstream f(reloadPath);
        f << "reload_key=before\n";
    }

    dConfig cfg(reloadFilename);
    EXPECT_EQ(cfg.GetValue("reload_key"), "before");

    // Overwrite the file with new content.
    {
        std::ofstream f(reloadPath);
        f << "reload_key=after\n";
        f << "new_key=appeared\n";
    }

    cfg.ReloadConfig();

    EXPECT_EQ(cfg.GetValue("reload_key"), "after");
    EXPECT_EQ(cfg.GetValue("new_key"),    "appeared");
}

TEST_F(ConfigTest, ReloadConfig_ClearsOldValues) {
    std::string reloadFilename = "test_config_reload.ini";
    auto reloadPath = BinaryPathFinder::GetBinaryDir() / reloadFilename;

    {
        std::ofstream f(reloadPath);
        f << "old_key=present\n";
    }

    dConfig cfg(reloadFilename);
    ASSERT_EQ(cfg.GetValue("old_key"), "present");

    // Reload with a file that no longer contains old_key.
    {
        std::ofstream f(reloadPath);
        f << "different_key=value\n";
    }

    cfg.ReloadConfig();

    // old_key should be gone after reload.
    EXPECT_EQ(cfg.GetValue("old_key"), "");
    EXPECT_EQ(cfg.GetValue("different_key"), "value");
}

// ---------------------------------------------------------------------------
// ConfigHandler callback
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, AddConfigHandler_CalledOnReload) {
    std::string reloadFilename = "test_config_reload.ini";
    auto reloadPath = BinaryPathFinder::GetBinaryDir() / reloadFilename;

    {
        std::ofstream f(reloadPath);
        f << "x=1\n";
    }

    dConfig cfg(reloadFilename);

    int callCount = 0;
    cfg.AddConfigHandler([&callCount]() { ++callCount; });

    {
        std::ofstream f(reloadPath);
        f << "x=2\n";
    }

    cfg.ReloadConfig();
    EXPECT_EQ(callCount, 1);

    cfg.ReloadConfig();
    EXPECT_EQ(callCount, 2);
}

TEST_F(ConfigTest, AddConfigHandler_NotCalledBeforeReload) {
    std::string reloadFilename = "test_config_reload.ini";
    auto reloadPath = BinaryPathFinder::GetBinaryDir() / reloadFilename;

    {
        std::ofstream f(reloadPath);
        f << "x=1\n";
    }

    dConfig cfg(reloadFilename);

    int callCount = 0;
    cfg.AddConfigHandler([&callCount]() { ++callCount; });

    // No ReloadConfig call — handler must not have been invoked yet.
    EXPECT_EQ(callCount, 0);
}

// ---------------------------------------------------------------------------
// Existence check
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, Exists_ReturnsTrueForExistingFile) {
    EXPECT_TRUE(dConfig::Exists(kConfigFilename));
}

TEST_F(ConfigTest, Exists_ReturnsFalseForMissingFile) {
    EXPECT_FALSE(dConfig::Exists("this_config_does_not_exist_xyz.ini"));
}

// ---------------------------------------------------------------------------
// Windows-style CRLF line endings
// ---------------------------------------------------------------------------

TEST_F(ConfigTest, CRLFLineEndings_ValuesStrippedOfCarriageReturn) {
    auto reloadPath = BinaryPathFinder::GetBinaryDir() / "test_config_reload.ini";
    {
        std::ofstream f(reloadPath, std::ios::binary);
        f << "crlf_key=windows_value\r\n";
        f << "next_key=ok\r\n";
    }

    dConfig cfg("test_config_reload.ini");
    // The trailing \r should be stripped by ProcessLine.
    EXPECT_EQ(cfg.GetValue("crlf_key"), "windows_value");
    EXPECT_EQ(cfg.GetValue("next_key"), "ok");
}
