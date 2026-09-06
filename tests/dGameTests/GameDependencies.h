#ifndef __GAMEDEPENDENCIES__H__
#define __GAMEDEPENDENCIES__H__

#include "Game.h"
#include "Logger.h"
#include "dServer.h"
#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "EntityInfo.h"
#include "EntityManager.h"
#include "dConfig.h"
#include "dZoneManager.h"
#include "GameDatabase/TestSQL/TestSQLDatabase.h"
#include "Database.h"
#include <gtest/gtest.h>
#include <string>

class dZoneManager;
class AssetManager;

class dServerMock : public dServer {
	RakNet::BitStream* sentBitStream = nullptr;
public:
	dServerMock() {};
	~dServerMock() {};
	RakNet::BitStream* GetMostRecentBitStream() { return sentBitStream; };
	void Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast) override { sentBitStream = &bitStream; };
	void SetZoneId(unsigned int zoneId) { mZoneID = zoneId; }
};

class GameDependenciesTest : public ::testing::Test {
protected:
	void SetUpDependencies() {
		info.pos = NiPoint3Constant::ZERO;
		info.rot = QuatUtils::IDENTITY;
		info.scale = 1.0f;
		info.spawner = nullptr;
		info.lot = 999;
		Game::logger = new Logger("./testing.log", true, true);
		Game::server = new dServerMock();
		Game::config = new dConfig("worldconfig.ini");
		Game::entityManager = new EntityManager();
		Game::zoneManager = new dZoneManager();
		Game::zoneManager->LoadZone(LWOZONEID(1, 0, 0));
		Database::_setDatabase(new TestSQLDatabase()); // this new is managed by the Database

#ifdef CDCLIENT_TEST_PATH
		// Open the real CDServer.sqlite so component constructors that issue
		// CDClientDatabase::CreatePreppedStmt(...) can query against it.
		// The file is gitignored and operator-supplied; skip Connect if it is
		// missing so we do not create an empty sqlite that then fails queries.
		if (!CDClientDatabase::isConnected && CdClientSqliteExists()) {
			CDClientDatabase::Connect(CDCLIENT_TEST_PATH);
		}
#endif

		// Create a CDClientManager instance and load from defaults
		CDClientManager::LoadValuesFromDefaults();

		// Mark the fixture entity LOT as visited so InventoryComponent (and
		// other GetByIDAndType callers) do not fall through to CDClient SQL
		// when resServer/CDServer.sqlite is not present.
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>()[static_cast<uint64_t>(info.lot)] = 0;
	}

	void TearDownDependencies() {
		if (Game::server) {
			delete Game::server;
			Game::server = nullptr;
		}
		if (Game::entityManager) {
			delete Game::entityManager;
			Game::entityManager = nullptr;
		}
		if (Game::zoneManager) {
			delete Game::zoneManager;
			Game::zoneManager = nullptr;
		}
		if (Game::logger) {
			Game::logger->Flush();
			delete Game::logger;
			Game::logger = nullptr;
		}
		if (Game::config) {
			delete Game::config;
			Game::config = nullptr;
		}
	}

	EntityInfo info{};

	// True when resServer/CDServer.sqlite is present on disk. Tests that only
	// inject CDClientManager entries do not need this; paths that issue SQL
	// (CheckItemSet, EquipItem, RequiresItem, etc.) must skip without it.
	static bool CdClientSqliteExists();

	static bool CdClientHasTable(const char* tableName);
};

// GTEST_SKIP() must run in the test body or fixture SetUp (it returns from
// the current function). Wrapping it in a helper would only return from the
// helper.
#define SKIP_IF_NO_CDCLIENT_SQLITE() \
	do { \
		if (!GameDependenciesTest::CdClientSqliteExists()) { \
			GTEST_SKIP() << "CDClient fixture missing (resServer/CDServer.sqlite)"; \
		} \
	} while (0)

#define SKIP_IF_NO_CDCLIENT_TABLE(tableName) \
	do { \
		if (!GameDependenciesTest::CdClientHasTable(tableName)) { \
			GTEST_SKIP() << "CDClient fixture missing (resServer/CDServer.sqlite); table '" \
				<< (tableName) << "' is unavailable"; \
		} \
	} while (0)

#endif //!__GAMEDEPENDENCIES__H__
