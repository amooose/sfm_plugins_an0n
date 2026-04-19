// Auto patches the keyframe hitbox based on size.cfg.
// Also makes an attempt at improving the click accuracy on the timeline by 1-2 pixels by adjusting the
// by an0nymooose
// 
// Plugin loader and Plugin template by ficool2

#include "plugin.h"
#include "patch.h"
#include <cmath>
#include <filesystem>
#include <windows.h>
#include <fstream>
#include "pluginCleanup.h"

class CPlugin_ScriptUpload : public IServerPluginCallbacks
{
	bool Load( CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory ) override;
	void Unload( void ) override {}
	void Pause( void ) override {}
	void UnPause( void ) override {}
	const char* GetPluginDescription( void ) override { return "Keyframe Hitbox Patch"; }
	void LevelInit( char const* pMapName ) override {}
	void ServerActivate( edict_t* pEdictList, int edictCount, int clientMax ) override {}
	void GameFrame( bool simulating ) override {}
	void LevelShutdown( void ) override {}
	void ClientActive( edict_t* pEntity ) override {}
	void ClientFullyConnect( edict_t* pEntity ) override {}
	void ClientDisconnect( edict_t* pEntity ) override {}
	void ClientPutInServer( edict_t* pEntity, char const* playername ) override {}
	void SetCommandClient( int index ) override {}
	void ClientSettingsChanged( edict_t* pEdict ) override {}
	PLUGIN_RESULT ClientConnect( bool* bAllowConnect, edict_t* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen ) override { return PLUGIN_CONTINUE; }
	PLUGIN_RESULT ClientCommand( edict_t* pEntity, const CCommand& args ) override { return PLUGIN_CONTINUE; }
	PLUGIN_RESULT NetworkIDValidated( const char* pszUserName, const char* pszNetworkID ) override { return PLUGIN_CONTINUE; }
	void OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t* pPlayerEntity, int eStatus, const char* pCvarName, const char* pCvarValue ) override {}
	void OnEdictAllocated( edict_t* edict ) override {}
	void OnEdictFreed( const edict_t* edict )  override {}
};
int PLUGIN_VERSION = 1;
int __stdcall convertScaledIntToCoord(int thisPtr, int scaledValue)
{
	// original logic and values
	if (scaledValue == -2147483647) return -1073741824;
	if (scaledValue == 0x7FFFFFFF)  return 0x3FFFFFFF;
	double baseOffset = (double)*(int*)(thisPtr + 16);
	float scaleFactor = *(float*)(thisPtr + 20);

	// prevent division by zero
	if (std::abs(scaleFactor) < 1e-9f) return (int)baseOffset;
	long double denominator = 10000.0L * (long double)scaleFactor;
	long double pixelsPerUnit = 1.0L / denominator;
	long double delta = (long double)scaledValue * pixelsPerUnit;
	long double finalCoord = (long double)baseOffset + delta;

	//magic, offset the coord a bit to compensate for precision loss when zooming in on the graph editor
	//Without this, when zoomed far in, 2-3 pixels can be offset
	//This usually brings it down to 0-1
	return (int)std::roundl(finalCoord - 0.50000000001L);
}

int __fastcall hookedConvertScaledIntToCoord(int thisPtr, void* edx, int scaledValue)
{
	return convertScaledIntToCoord(thisPtr, scaledValue);
}


int GetConfigInt(const std::string& key) {
	std::ifstream file(std::filesystem::current_path() / "workshop/scripts/sfm/mainmenu/an0n/cfg/hitbox_size.cfg");
	if (!file.is_open()) return -1;

	std::string line;
	std::string searchKey = key + "=";

	while (std::getline(file, line)) {
		if (line.compare(0, searchKey.length(), searchKey) == 0) {
			try {
				return std::stoi(line.substr(searchKey.length()));
			}
			catch (...) {
				return -1;
			}
		}
	}
	return -1;
}

void patchHitbox(uintptr_t base) {
	uintptr_t base_addr = (uintptr_t)base;

	//try to load saved config to auto apply
	int size = GetConfigInt("size");
	if (size == 0) {
		return;
	}

	// calculate hitbox values
	unsigned char right_val = (unsigned char)((((size) * -1) - 3) & 0xFF);
	unsigned char left_val = (unsigned char)(size + 4);

	Patch::WriteBytes((void*)(base_addr + 0x19C5CC), &right_val, sizeof(right_val));
	Patch::WriteBytes((void*)(base_addr + 0x19C5DB), &left_val, sizeof(left_val));
}

bool CPlugin_ScriptUpload::Load( CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory )
{
	LOG( "Keyframe Hitbox Patch loading...\n" );

	auto ifm_base = ( unsigned char* )GetModuleHandle( "ifm" );

	if ( ifm_base == NULL )
	{
		LOG( "Failed to find ifm\n" );
		return false;
	}

	{
		uintptr_t addr3 = (uintptr_t)ifm_base + 0x1651E0;
		int no_px = GetConfigInt("no_px");
		if (no_px != 1) {

			Patch::WriteJump((void*)addr3, (void*)hookedConvertScaledIntToCoord, 0);
		}
		std::string cwd = std::filesystem::current_path().string();
		std::filesystem::path vdfPath = std::filesystem::current_path() / "workshop/addons/timelineprecisionpatch.vdf";

		cleanupPlugins(vdfPath.string());
		patchHitbox((uintptr_t)ifm_base);

	}

	return true;
}

static CPlugin_ScriptUpload g_PluginScriptUpload;
IServerPluginCallbacks* g_pPlugin = &g_PluginScriptUpload;