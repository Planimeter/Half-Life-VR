#pragma once

#include <unordered_map>
#include <string>
#include <chrono>


class VRSettings
{
public:

	void Init();

	void InitialUpdateCVARSFromJson();

	void CheckCVARsForChanges();

private:
	enum class RetryMode
	{
		NONE,
		WRITE,
		LOAD
	};

	void RegisterCVAR(const char* name, const char* value);
	void SetCVAR(const char* name, const char* value);
	void UpdateCVARSFromJson();
	void UpdateJsonFromCVARS();
	bool WasSettingsFileChanged();
	bool WasAnyCVARChanged();

	std::unordered_map<std::string, std::string>			m_cvarCache;
	void*													m_watchVRFolderHandle;
	RetryMode												m_needsRetry{ RetryMode::NONE };
	long long												m_lastSettingsFileChangedTime{ 0 };
	long long												m_nextSettingCheckTime{ 0 };
};

extern VRSettings g_vrSettings;
