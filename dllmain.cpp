#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int) (short) HIWORD(lp))

#include <windows.h>
#include <d3d9.h>
#include <string>
#include <vector>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx9.h"
#include "ImGui/imgui_impl_win32.h"
#include "detours.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "detours.lib")

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef HRESULT(WINAPI* fnEndScene)(LPDIRECT3DDEVICE9);
typedef HRESULT(APIENTRY* fnReset)(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);

struct sGameWindow
{
	HWND hwOGGameWindow;
	HWND hwGameWindow;
	WNDPROC wpOriginalWndProc;
	const char* szGameTitlePart;
	const char* szGameClass;
	const char* szChildClass;
	bool bIsGuiOpen;
	LPDIRECT3DDEVICE9 pDevice;

	sGameWindow()
		: hwOGGameWindow(NULL), hwGameWindow(NULL), wpOriginalWndProc(NULL),
		szGameTitlePart("Conquer"),
		szGameClass("Afx:00400000:0:000100"), szChildClass("#32770"),
		bIsGuiOpen(true), pDevice(nullptr)
	{
	}
};

sGameWindow gGameWindow;

fnEndScene gfnOEndScene = nullptr;
fnReset gfnOReset = nullptr;
bool gbInitEndScene = false;

uintptr_t findPattern(uintptr_t uStart, size_t szLength, const std::vector<int>& vPattern)
{
	size_t szPatternLength = vPattern.size();
	const uint8_t* pData = reinterpret_cast<const uint8_t*>(uStart);

	for (size_t i = 0; i <= szLength - szPatternLength; ++i)
	{
		bool bFound = true;

		for (size_t j = 0; j < szPatternLength; ++j)
		{
			if (vPattern[j] != -1 && vPattern[j] != pData[i + j])
			{
				bFound = false;
				break;
			}
		}

		if (bFound)
		{
			return uStart + i;
		}
	}

	return 0;
}

HWND findGameWindow()
{
	HWND hwParentHandle = NULL;

	auto fnEnumProc = [](HWND hWnd, LPARAM lParam) -> BOOL
		{
			DWORD dwProcId = 0;
			GetWindowThreadProcessId(hWnd, &dwProcId);

			char szWindowTitle[256] = { 0 };
			GetWindowTextA(hWnd, szWindowTitle, sizeof(szWindowTitle));

			char szClassName[256] = { 0 };
			GetClassNameA(hWnd, szClassName, sizeof(szClassName));

			if (GetCurrentProcessId() == dwProcId &&
				(strstr(szWindowTitle, gGameWindow.szGameTitlePart) != nullptr ||
					strstr(szClassName, gGameWindow.szGameClass) != nullptr))
			{
				*reinterpret_cast<HWND*>(lParam) = hWnd;
				return FALSE;
			}

			return TRUE;
		};

	EnumWindows(fnEnumProc, reinterpret_cast<LPARAM>(&hwParentHandle));

	if (hwParentHandle != NULL)
	{
		gGameWindow.hwOGGameWindow = hwParentHandle;
		HWND hwChildHandle = FindWindowExA(hwParentHandle, NULL, gGameWindow.szChildClass, NULL);

		if (hwChildHandle != NULL)
		{
			return hwChildHandle;
		}
	}

	return NULL;
}

void renderGui()
{
	if (!gGameWindow.bIsGuiOpen)
		return;

	static bool demoWindow = true;

	if (demoWindow)
		ImGui::ShowDemoWindow();

	ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);

	ImGui::Begin("CONQUER - IMGUI");
	{
		if (ImGui::Button("HIDE"))
			gGameWindow.bIsGuiOpen = false;

		ImGui::Separator();
		ImGui::Text("Hello from ImGui!");

		static char szInputText[256] = "";
		ImGui::InputText("Text Input", szInputText, sizeof(szInputText));

		static int iSliderValue = 0;
		ImGui::SliderInt("Slider", &iSliderValue, 0, 100);

		static float fSliderValue = 0.5f;
		ImGui::SliderFloat("Float Slider", &fSliderValue, 0.0f, 1.0f);

		static bool bCheckbox = false;
		ImGui::Checkbox("Checkbox", &bCheckbox);

		ImGui::Separator();
		ImGui::Checkbox("Demo window", &demoWindow);
	}
	ImGui::End();
}

HRESULT APIENTRY hkEndScene(LPDIRECT3DDEVICE9 pDevice)
{
	if (!gGameWindow.pDevice)
	{
		gGameWindow.pDevice = pDevice;
		if (!gGameWindow.pDevice)
		{
			return gfnOEndScene(pDevice);
		}
	}

	if (!gbInitEndScene)
	{
		ImGui::CreateContext();
		ImGui_ImplWin32_Init(gGameWindow.hwOGGameWindow);
		ImGui_ImplDX9_Init(gGameWindow.pDevice);
		gbInitEndScene = true;
	}

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	renderGui();

	ImGui::EndFrame();
	ImGui::Render();

	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

	return gfnOEndScene(pDevice);
}

HRESULT APIENTRY hkReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	gbInitEndScene = false;
	DetourDetach(&(PVOID&)gfnOEndScene, hkEndScene);
	DetourTransactionCommit();

	HRESULT hr = gfnOReset(pDevice, pPresentationParameters);

	if (SUCCEEDED(hr))
	{
		ImGui_ImplDX9_CreateDeviceObjects();
		ImGui_ImplDX9_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)gfnOEndScene, hkEndScene);
		DetourTransactionCommit();
	}

	return gfnOReset(pDevice, pPresentationParameters);
}

LRESULT CALLBACK hookedWndProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, uMessage, wParam, lParam))
	{
		return false;
	}

	ImGuiIO& io = ImGui::GetIO();

	switch (uMessage)
	{
	case WM_MOUSEMOVE:
	{
		int iMouseX = GET_X_LPARAM(lParam);
		int iMouseY = GET_Y_LPARAM(lParam);
		io.MousePos = ImVec2((float)iMouseX, (float)iMouseY);

		if (io.WantCaptureMouse)
		{
			return 0;
		}
		break;
	}

	case WM_LBUTTONDOWN:
	{
		io.MouseDown[0] = true;
		if (io.WantCaptureMouse)
		{
			return 0;
		}
		break;
	}

	case WM_LBUTTONUP:
	{
		io.MouseDown[0] = false;
		if (io.WantCaptureMouse)
		{
			return 0;
		}
		break;
	}

	case WM_RBUTTONDOWN:
	{
		io.MouseDown[1] = true;
		if (io.WantCaptureMouse)
		{
			return 0;
		}
		break;
	}

	case WM_RBUTTONUP:
	{
		io.MouseDown[1] = false;
		if (io.WantCaptureMouse)
		{
			return 0;
		}
		break;
	}

	case WM_MBUTTONDOWN:
	{
		io.MouseDown[2] = true;
		if (io.WantCaptureMouse)
		{
			return 0;
		}
		break;
	}

	case WM_MBUTTONUP:
	{
		io.MouseDown[2] = false;
		if (io.WantCaptureMouse)
		{
			return 0;
		}
		break;
	}

	case WM_MOUSEWHEEL:
	{
		int iDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		io.MouseWheel += (float)iDelta / WHEEL_DELTA;

		if (io.WantCaptureMouse)
		{
			return 0;
		}
		break;
	}

	case WM_MOUSEHWHEEL:
	{
		int iDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		io.MouseWheelH += (float)iDelta / WHEEL_DELTA;

		if (io.WantCaptureMouse)
		{
			return 0;
		}
		break;
	}

	case WM_MOUSELEAVE:
	{
		io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
		break;
	}
	}

	return CallWindowProcA(gGameWindow.wpOriginalWndProc, hWnd, uMessage, wParam, lParam);
}

//installing the window hook on #32770 instead of the main
void installWindowHook()
{
	while (!gGameWindow.hwGameWindow)
	{
		if (gGameWindow.hwGameWindow == NULL)
		{
			gGameWindow.hwGameWindow = findGameWindow();
		}

		if (gGameWindow.hwGameWindow != NULL && gGameWindow.wpOriginalWndProc == NULL)
		{
			gGameWindow.wpOriginalWndProc = (WNDPROC)SetWindowLongA(
				gGameWindow.hwGameWindow, GWLP_WNDPROC, (LONG_PTR)hookedWndProc);
		}
	}
}

void hookThread()
{
	while (GetModuleHandleA("d3d9.dll") == nullptr)
	{
		Sleep(100);
	}

	uintptr_t uD3d9Module = reinterpret_cast<uintptr_t>(GetModuleHandleA("d3d9.dll"));

	std::vector<int> vPattern =
	{
		0xC7, 0x06, -1, -1, -1, -1, 0x89, 0x86, -1, -1, -1, -1, 0x89, 0x86
	};

	uintptr_t uD3dBase = findPattern(uD3d9Module, (sizeof(vPattern) / 2), vPattern);

	if (uD3dBase == 0)
	{
		MessageBoxA(NULL, "Failed to find D3D pattern.", "Error", MB_OK | MB_ICONERROR);
		return;
	}

	uintptr_t* pD3dVMT = *reinterpret_cast<uintptr_t**>(uD3dBase + 2);

	gfnOEndScene = reinterpret_cast<fnEndScene>(pD3dVMT[42]);
	gfnOReset = reinterpret_cast<fnReset>(pD3dVMT[16]);

	DetourRestoreAfterWith();
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	DetourAttach(&(PVOID&)gfnOEndScene, hkEndScene);
	DetourAttach(&(PVOID&)gfnOReset, hkReset);

	//Sleep(3000);
	installWindowHook();

	DetourTransactionCommit();

	while (true)
	{
		Sleep(16);
		if (GetAsyncKeyState(VK_INSERT) & 1)
		{
			gGameWindow.bIsGuiOpen = !gGameWindow.bIsGuiOpen;
			Sleep(200);
		}
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
	{
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)hookThread, NULL, 0, NULL);
		break;
	}
	}

	return TRUE;
}
