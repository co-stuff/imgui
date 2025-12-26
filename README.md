IMGUI for ConquerOnline (D3D9 & compatible w/ D3D8to9 on newer clients)
* Tested on 6609 and latest game versions
* Compile on release & x86
* ImGui version used 16602, consider to use a newer one
* If you dont want to use the detours.lib included in the project, compile yourself from the official repo: https://github.com/microsoft/Detours

Known issues:
* Using the og windows (parent/child) from the game may be a little silly, this leads to wrong functionality of ImGui_ImplWin32_WndProcHandler
* WndProc isnt getting correctly keys (only mouse), you have to map keys w/ other function
* Old imgui version...
