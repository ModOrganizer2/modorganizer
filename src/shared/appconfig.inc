APPPARAM(std::wstring, translationPrefix, L"organizer")
APPPARAM(std::wstring, pluginPath, L"plugins")
APPPARAM(std::wstring, stylesheetsPath, L"stylesheets")
APPPARAM(std::wstring, profileTweakIni, L"profile_tweaks.ini")
APPPARAM(std::wstring, logFile, L"ModOrganizer.log")
APPPARAM(std::wstring, proxyDLLTarget, L"steam_api.dll")
APPPARAM(std::wstring, proxyDLLOrig, L"steam_api_orig.dll") // needs to be identical to the value used in proxydll-project
APPPARAM(std::wstring, proxyDLLSource, L"proxy.dll")
APPPARAM(std::wstring, hookDLLName, L"hook.dll") // needs to be identical to the value used in proxydll-project
APPPARAM(const wchar_t*, localSavePlaceholder, L"__MOProfileSave__\\")

APPPARAM(std::wstring, firstStepsTutorial, L"tutorial_firststeps_main.js")
APPPARAM(int, numLogFiles, 5) // number of log files to keep
