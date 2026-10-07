#include "VanityUtilities.h"

#include "CDClientDatabase.h"
#include "GameMessages.h"
#include "CppScripts.h"
#include "BankInteractServer.h"
#include "tinyxml2.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>

namespace {
	const std::filesystem::path vanityDir = VANITY_TEST_PATH;

	size_t CountAtmLocations(uint32_t zoneID) {
		tinyxml2::XMLDocument document;
		if (document.LoadFile((vanityDir / "atm.xml").string().c_str()) != tinyxml2::XML_SUCCESS) return 0;
		auto* object = document.FirstChildElement("objects")->FirstChildElement("object");
		size_t count = 0;
		for (auto* location = object->FirstChildElement("locations")->FirstChildElement("location");
			location; location = location->NextSiblingElement("location")) {
			if (location->UnsignedAttribute("zone") == zoneID) ++count;
		}
		return count;
	}

	size_t CountAtms(const std::vector<VanityObject>& objects) {
		size_t count = 0;
		for (const auto& object : objects) {
			if (object.m_LOT == 13538) ++count;
		}
		return count;
	}

	class TemporaryVanityFiles {
	public:
		TemporaryVanityFiles() {
			directory = std::filesystem::current_path() /
				("dfs-vanity-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
			std::filesystem::create_directories(directory);
		}

		~TemporaryVanityFiles() { std::filesystem::remove_all(directory); }

		std::filesystem::path Write(const std::string& name, const std::string& contents) const {
			auto path = directory / name;
			std::ofstream file(path);
			file << contents;
			return path;
		}

		void Copy(const std::filesystem::path& source, const std::string& name) const {
			std::filesystem::copy_file(source, directory / name);
		}

	private:
		std::filesystem::path directory;
	};
}

TEST(VanityUtilitiesTests, ShippedAtmsSpawnAtEveryLocationWithVaultPrecondition) {
	for (uint32_t zoneID : { 1200u, 1100u, 1800u }) {
		const auto objects = VanityUtilities::ParseVanity(vanityDir / "root.xml", zoneID);
		const auto expected = CountAtmLocations(zoneID);
		ASSERT_GT(expected, 0u);
		ASSERT_EQ(CountAtms(objects), expected) << "zone " << zoneID;
		for (const auto& object : objects) {
			if (object.m_LOT != 13538) continue;
			ASSERT_EQ(object.m_Locations.at(zoneID).size(), 1u);
			bool hasPrecondition = false;
			for (const auto* config : object.m_Config) {
				if (config->GetString() == "CheckPrecondition=0:168") hasPrecondition = true;
			}
			EXPECT_TRUE(hasPrecondition);
		}
	}
}

TEST(VanityUtilitiesTests, VentureExplorerHasNoAtms) {
	const auto objects = VanityUtilities::ParseVanity(vanityDir / "root.xml", 1000);
	EXPECT_EQ(CountAtms(objects), 0u);
}

TEST(VanityUtilitiesTests, DisabledRootOmitsAtms) {
	TemporaryVanityFiles files;
	files.Copy(vanityDir / "atm.xml", "atm.xml");
	const auto root = files.Write("root.xml", "<files><file name=\"atm.xml\" enabled=\"0\"/></files>");
	const auto objects = VanityUtilities::ParseVanity(root, 1200);
	EXPECT_EQ(CountAtms(objects), 0u);
}

TEST(VanityUtilitiesTests, RandomSpawnFlagUsesItsBooleanValue) {
	std::ifstream source(vanityDir / "atm.xml");
	ASSERT_TRUE(source.good());
	const std::string atmXml((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
	const std::string precondition = "<key>CheckPrecondition=0:168</key>";
	ASSERT_NE(atmXml.find(precondition), std::string::npos);

	TemporaryVanityFiles files;
	const auto root = files.Write("root.xml", "<files><file name=\"atm.xml\" enabled=\"1\"/></files>");
	const auto expected = CountAtmLocations(1200);
	ASSERT_GT(expected, 1u);

	for (const auto& [value, objectCount, locationCount] : {
		std::tuple{ "0", expected, size_t{1} },
		std::tuple{ "1", size_t{1}, expected }
	}) {
		std::string xml = atmXml;
		xml.insert(xml.find(precondition) + precondition.size(),
			std::string("<key>useLocationsAsRandomSpawnPoint=7:") + value + "</key>");
		files.Write("atm.xml", xml);
		const auto objects = VanityUtilities::ParseVanity(root, 1200);
		ASSERT_EQ(CountAtms(objects), objectCount) << "flag " << value;
		for (const auto& object : objects) {
			if (object.m_LOT == 13538) EXPECT_EQ(object.m_Locations.at(1200).size(), locationCount);
		}
	}
}

TEST(VanityUtilitiesTests, CatalogAtmUsesBankInteractServer) {
	if (!std::filesystem::exists(CDCLIENT_TEST_PATH)) {
		GTEST_SKIP() << "CDClient fixture missing (resServer/CDServer.sqlite)";
	}
	if (!CDClientDatabase::isConnected) CDClientDatabase::Connect(CDCLIENT_TEST_PATH);

	auto result = CDClientDatabase::ExecuteQuery(
		"SELECT ScriptComponent.script_name FROM ComponentsRegistry "
		"JOIN ScriptComponent ON ComponentsRegistry.component_id = ScriptComponent.id "
		"WHERE ComponentsRegistry.id = 13538 AND ComponentsRegistry.component_type = 5");
	ASSERT_FALSE(result.eof());
	const std::string scriptName = result.getStringField("script_name", "");
	result.finalize();
	ASSERT_EQ(scriptName, "scripts\\02_server\\Map\\General\\L_BANK_INTERACT_SERVER.lua");
	EXPECT_NE(dynamic_cast<BankInteractServer*>(CppScripts::GetScript(nullptr, scriptName)), nullptr);
}
