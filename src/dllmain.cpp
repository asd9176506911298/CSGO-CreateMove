#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>

#include "../ext/minhook/MinHook.h"
#include "usercmd.h"

template <typename T>
T* GetInterface(const char* name, const char* dllname)
{
	const auto handle = GetModuleHandle(dllname);

	if (!handle)
		return nullptr;

	auto functionAddress = GetProcAddress(handle, "CreateInterface");

	if (!functionAddress)
		return nullptr;;

	using Fn = T * (*)(const char*, int*);
	const auto CreateInterface = reinterpret_cast<Fn>(functionAddress);

	return CreateInterface(name, nullptr);
}

static void* g_Client = nullptr;
static void* g_ClientMode = nullptr;

using CreateMove = bool(__thiscall*)(void*, float, UserCmd*);
static CreateMove CreateMoveOriginal = nullptr;

constexpr unsigned int dwLocalPlayer = 0xDEA964;
constexpr unsigned int m_fFlags = 0x104;

bool __stdcall CreateMoveHook(float flInputSampleTime, UserCmd* cmd)
{
	const bool result = CreateMoveOriginal(g_ClientMode, flInputSampleTime, cmd);
	
	if (!cmd || !cmd->command_number)
		return result;

	static unsigned int client = reinterpret_cast<unsigned int>(GetModuleHandle("client.dll"));

	const unsigned int localPlayer = *reinterpret_cast<unsigned int*>(client + dwLocalPlayer);

	if (localPlayer)
	{
		if (!(*reinterpret_cast<int*>(localPlayer + m_fFlags) & FL_ONGROUND))
		{
			cmd->buttons &= ~IN_JUMP;
			
		};

		cmd->buttons |= IN_BULLRUSH;
		
	}

	return false;
}

DWORD WINAPI HackThread(HMODULE hModule)
{
	AllocConsole();
	FILE* f;
	freopen_s(&f, "CONOUT$", "w", stdout);

	auto g_Client = GetInterface<void>("VClient018", "client.dll");

	g_ClientMode = **reinterpret_cast<void***>((*reinterpret_cast<unsigned int**>(g_Client))[10] + 5);

	MH_Initialize();

	MH_CreateHook(
		(*static_cast<void***>(g_ClientMode))[24],
		&CreateMoveHook,
		reinterpret_cast<void**>(&CreateMoveOriginal)
	);

	MH_EnableHook(MH_ALL_HOOKS);

	while (!GetAsyncKeyState(VK_END))
		Sleep(200);

	MH_DisableHook(MH_ALL_HOOKS);
	MH_RemoveHook(MH_ALL_HOOKS);
	MH_Uninitialize();

	FreeConsole();
	fclose(f);
	FreeLibraryAndExitThread(hModule, 0);
	return true;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD reason, LPVOID reserve)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);
		auto thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)HackThread, hModule, 0, 0);

		if (thread)
			CloseHandle(thread);
	}

	return true;
}