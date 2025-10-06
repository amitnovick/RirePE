#ifndef __DEBUG_LOG_H__
#define __DEBUG_LOG_H__

#include <Windows.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>

// Simple file-based logging for debugging
class DebugLog {
private:
    static std::wstring GetLogPath() {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        std::wstring wPath(path);
        size_t pos = wPath.find_last_of(L"\\");
        if (pos != std::wstring::npos) {
            wPath = wPath.substr(0, pos + 1);
        }
        return wPath + L"RirePE_Debug.log";
    }

public:
    static void Log(const std::wstring& message) {
        std::wofstream logFile;
        logFile.open(GetLogPath(), std::ios::app);
        if (logFile.is_open()) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            logFile << std::setfill(L'0') << std::setw(2) << st.wHour << L":"
                    << std::setw(2) << st.wMinute << L":"
                    << std::setw(2) << st.wSecond << L"."
                    << std::setw(3) << st.wMilliseconds << L" - "
                    << message << std::endl;
            logFile.close();
        }
    }

    static void LogHex(const std::wstring& label, ULONG_PTR value) {
        std::wstringstream ss;
        ss << label << L": 0x" << std::hex << std::uppercase << std::setfill(L'0')
#ifdef _WIN64
           << std::setw(16)
#else
           << std::setw(8)
#endif
           << value;
        Log(ss.str());
    }

    static void Clear() {
        std::wofstream logFile;
        logFile.open(GetLogPath(), std::ios::trunc);
        logFile.close();
    }
};

// Macro for easy logging
#define DEBUGLOG(msg) DebugLog::Log(msg)
#define DEBUGLOGHEX(label, value) DebugLog::LogHex(label, value)

#endif // __DEBUG_LOG_H__
