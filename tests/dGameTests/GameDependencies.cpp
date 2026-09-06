#include "GameDependencies.h"

#include <filesystem>

bool GameDependenciesTest::CdClientSqliteExists() {
#ifdef CDCLIENT_TEST_PATH
	return std::filesystem::exists(std::filesystem::path{CDCLIENT_TEST_PATH});
#else
	return false;
#endif
}

bool GameDependenciesTest::CdClientHasTable(const char* tableName) {
	if (!CDClientDatabase::isConnected) {
		return false;
	}

	try {
		const std::string sql = std::string("SELECT name FROM sqlite_master WHERE type='table' AND name='")
			+ tableName + "';";
		auto result = CDClientDatabase::ExecuteQuery(sql);
		const bool exists = !result.eof();
		result.finalize();
		return exists;
	} catch (...) {
		return false;
	}
}

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dZoneManager* zoneManager = nullptr;
	dChatFilter* chatFilter = nullptr;
	dConfig* config = nullptr;
	std::mt19937 randomEngine;
	RakPeerInterface* chatServer = nullptr;
	AssetManager* assetManager = nullptr;
	SystemAddress chatSysAddr;
	EntityManager* entityManager = nullptr;
	std::string projectVersion;
	Game::signal_t lastSignal = 0;
}
