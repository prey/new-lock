#define NTDDI_VERSION (0x06020000)
#define _WIN32_WINNT (0x0602)
#if defined(UNICODE) && !defined(_UNICODE)
#define _UNICODE
#elif !defined(UNICODE) && defined(_UNICODE)
#define UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include <wtsapi32.h>
#include <tchar.h>
#include <sddl.h>
#include <iostream>
#include "get_option.h"

#define REGISTRY_SETTINGS_LOCATION "Software\\Prey\\lock-screen"

#ifndef ERROR_ELEVATION_REQUIRED
#define ERROR_ELEVATION_REQUIRED (740)
#endif // ERROR_ELEVATION_REQUIRED

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);

#ifdef UNICODE
typedef std::wstring tstring;
typedef std::wostringstream tostringstream;
typedef std::wistringstream tistringstream;
typedef wget_option tget_option;
#define Tcout (::std::wcout)
#define Tcerr (::std::wcerr)
#else
typedef std::string tstring;
typedef std::ostringstream tostringstream;
typedef std::istringstream tistringstream;
typedef get_option tget_option;
#define Tcout (::std::cout)
#define Tcerr (::std::cerr)
#endif // UNICODE

bool verbose = true;


struct PersistentSystemOptions final
{
  bool hasLockComputer;
  bool hasSwitchUser;
  bool hasLogOff;
  bool hasChangePassword;
  bool hasTaskManager;
  bool hasPower;
  bool hasFilterKeysHotkey;
  bool hasMouseKeysHotkey;
  bool hasStickyKeysHotkey;
  bool hasToggleKeysHotkey;
  bool hasFastBootHotKey;
  bool hasHighContrastHotkey;


  PersistentSystemOptions(bool enabled = true)
    : hasLockComputer(enabled),
    hasSwitchUser(enabled),
    hasLogOff(enabled),
    hasChangePassword(enabled),
    hasTaskManager(enabled),
    hasPower(enabled),
    hasFilterKeysHotkey(enabled),
    hasMouseKeysHotkey(enabled),
    hasStickyKeysHotkey(enabled),
    hasToggleKeysHotkey(enabled),
    hasFastBootHotKey(enabled),
    hasHighContrastHotkey(enabled)
  {
  }
  static PersistentSystemOptions AllDisabled()
  {
    return PersistentSystemOptions(false);
  }
  static PersistentSystemOptions AllEnabled()
  {
    return PersistentSystemOptions(true);
  }
  static PersistentSystemOptions ReadSystemSettings();
  bool WriteSystemSettings() const;
  static PersistentSystemOptions ReadFromStorage();
  bool WriteToStorage() const;
  std::ostream &Dump(std::ostream &os) const
  {
    os << "hasLockComputer = " << (hasLockComputer ? "true\n" : "false\n");
    os << "hasSwitchUser = " << (hasSwitchUser ? "true\n" : "false\n");
    os << "hasLogOff = " << (hasLogOff ? "true\n" : "false\n");
    os << "hasChangePassword = " << (hasChangePassword ? "true\n" : "false\n");
    os << "hasTaskManager = " << (hasTaskManager ? "true\n" : "false\n");
    os << "hasPower = " << (hasPower ? "true\n" : "false\n");
    os << "hasFilterKeysHotkey = " << (hasFilterKeysHotkey ? "true\n" : "false\n");
    os << "hasMouseKeysHotkey = " << (hasMouseKeysHotkey ? "true\n" : "false\n");
    os << "hasStickyKeysHotkey = " << (hasStickyKeysHotkey ? "true\n" : "false\n");
    os << "hasToggleKeysHotkey = " << (hasToggleKeysHotkey ? "true\n" : "false\n");
    os << "hasHighContrastHotkey = " << (hasHighContrastHotkey ? "true\n" : "false\n");
    os << "hasFastBootHotkey = " << (hasFastBootHotKey ? "true\n" : "false\n");
    return os;
  }
  std::wostream &Dump(std::wostream &os) const
  {
    os << L"hasLockComputer = " << (hasLockComputer ? L"true\n" : L"false\n");
    os << L"hasSwitchUser = " << (hasSwitchUser ? L"true\n" : L"false\n");
    os << L"hasLogOff = " << (hasLogOff ? L"true\n" : L"false\n");
    os << L"hasChangePassword = " << (hasChangePassword ? L"true\n" : L"false\n");
    os << L"hasTaskManager = " << (hasTaskManager ? L"true\n" : L"false\n");
    os << L"hasPower = " << (hasPower ? L"true\n" : L"false\n");
    os << L"hasFilterKeysHotkey = " << (hasFilterKeysHotkey ? L"true\n" : L"false\n");
    os << L"hasMouseKeysHotkey = " << (hasMouseKeysHotkey ? L"true\n" : L"false\n");
    os << L"hasStickyKeysHotkey = " << (hasStickyKeysHotkey ? L"true\n" : L"false\n");
    os << L"hasToggleKeysHotkey = " << (hasToggleKeysHotkey ? L"true\n" : L"false\n");
    os << L"hasHighContrastHotkey = " << (hasHighContrastHotkey ? L"true\n" : L"false\n");
    os << L"hasFastBootHotkey = " << (hasFastBootHotKey ? L"true\n" : L"false\n");
    return os;
  }
  private:
  static bool ReadRegistryKey(HKEY baseHive, tstring path, tstring name, bool defaultValue);
  static bool WriteRegistryKey(HKEY baseHive, tstring path, tstring name, bool value);
  static bool ReadFilterKeysHotkey();
  static bool WriteFilterKeysHotkey(bool value);
  static bool ReadMouseKeysHotkey();
  static bool WriteMouseKeysHotkey(bool value);
  static bool ReadStickyKeysHotkey();
  static bool WriteStickyKeysHotkey(bool value);
  static bool ReadToggleKeysHotkey();
  static bool WriteToggleKeysHotkey(bool value);
  static bool ReadHighContrastHotkey();
  static bool WriteHighContrastHotkey(bool value);
  static bool ReadFastBootHotkey();
  static bool WriteFastBootHotkey(bool value);
};



bool getActiveConsoleId(LPDWORD id) {
  PWTS_SESSION_INFO sessioninfo;
  DWORD count;

  if (!WTSEnumerateSessions (WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessioninfo, &count))
    return FALSE;

  // Ok, now find the active session

  DWORD i;
  for (i = 0; i < count; i++) {
    if (sessioninfo[i].State == WTSActive) {
      *id = sessioninfo[i].SessionId;
      break;
    }
  }

  WTSFreeMemory(&sessioninfo);
  return TRUE;
}

WCHAR* getSid() {

  DWORD sessionid;
  DWORD usersize = 0;
  WCHAR* username;

  if (!getActiveConsoleId(&sessionid)) {
    //error("WTSEnumerateSessions failed: %ld", GetLastError());
  } else {
    //info("WTSEnumerateSessions got active session: %d", sessionid);
  }

  if (!WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, sessionid, WTSUserName, &username, &usersize)) {
    //error("WTSQuerySessionInformation failed: %ld", GetLastError());
  } else {
    //info("WTSQuerySessionInformation got user: %s", username);
  }

  PSID sid = NULL;
  DWORD sid_size = 0;
  LPWSTR domain = NULL;
  DWORD domain_size = 0;
  SID_NAME_USE use;

  // First, we need to get the buffer size needed to store the SID
  if(!LookupAccountName(NULL, username, sid, &sid_size, domain, &domain_size, &use)) {
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      //error("LookupAccountName error while getting buffer size: %ld", GetLastError());
    }
  }

  try {
    sid = (PSID) LocalAlloc(LMEM_FIXED, sid_size);
    domain = (LPWSTR) malloc(domain_size * sizeof(LPWSTR));
    if ( !sid || !domain ) {
      throw(0);
    }
    if (!LookupAccountName(NULL, username, sid, &sid_size, domain, &domain_size, &use)) {
      //error("LookupAccountName error while getting buffer size: %ld", GetLastError());
      throw(0);
    }
  } catch (int) {
    LocalFree(sid);
    sid = NULL;
    //error("LookupAccountName error while getting actual data: %ld", GetLastError());
  }

  WCHAR* string_sid;
  if(!ConvertSidToStringSid(sid, &string_sid)) {
    //error("Could not convert to string from SID");
  }

  // do something with error
  return string_sid;
}

std::wstring s2ws(const std::string& str)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

PersistentSystemOptions PersistentSystemOptions::ReadSystemSettings()
{
  WCHAR *sid_w = getSid();

    std::wstring sid_ws(sid_w);
// your new String
   // std::string sid_str(sid_ws.begin(), sid_ws.end());
   // const char*sid=sid_str.c_str();

  std::wstring system = s2ws("\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System");
  std::wstring explorer = s2ws("\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer");
  std::wstring power = s2ws("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power");

  //if(! sid = getSid()) {
  //  return false;
  //}

  PersistentSystemOptions retval;
  retval.hasChangePassword = !ReadRegistryKey(HKEY_USERS, sid_ws + system, _T("DisableChangePassword"), false);
  retval.hasLockComputer = !ReadRegistryKey(HKEY_USERS, sid_ws + system, _T("DisableLockWorkstation"), false);
  retval.hasLogOff = !ReadRegistryKey(HKEY_USERS, sid_ws + explorer, _T("NoLogoff"), false);
  retval.hasSwitchUser = !ReadRegistryKey(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System"), _T("HideFastUserSwitching"), false);
  retval.hasTaskManager = !ReadRegistryKey(HKEY_USERS, sid_ws + system, _T("DisableTaskMgr"), false);
  retval.hasPower = !ReadRegistryKey(HKEY_USERS, sid_ws + explorer, _T("NoClose"), false);
  retval.hasFilterKeysHotkey = ReadFilterKeysHotkey();
  retval.hasMouseKeysHotkey = ReadMouseKeysHotkey();
  retval.hasStickyKeysHotkey = ReadStickyKeysHotkey();
  retval.hasToggleKeysHotkey = ReadToggleKeysHotkey();
  retval.hasHighContrastHotkey = ReadHighContrastHotkey();
  retval.hasFastBootHotKey = !ReadRegistryKey(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power"), _T("HiberbootEnabled"), false);
  return retval;
}

bool PersistentSystemOptions::WriteSystemSettings() const
{
  bool allWorked = true;
  WCHAR* sid_w = getSid();


    std::wstring sid_ws(sid_w);
  std::wstring system = s2ws("\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System");
  std::wstring explorer = s2ws("\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer");
  std::wstring power = s2ws("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power");

  //if(!sid = getSid()) {
  //  return false;
  //}

  if(!WriteRegistryKey(HKEY_USERS,sid_w + system, _T("DisableChangePassword"), !hasChangePassword)) {
    allWorked = false;
    Tcout << _T("Error DisableChangePassword") << std::endl;
  }

  if(!WriteRegistryKey(HKEY_USERS, sid_w + system, _T("DisableLockWorkstation"), !hasLockComputer)) {
    allWorked = false;
    Tcout << _T("Error DisableLockWorkstation") << std::endl;
  }

  if(!WriteRegistryKey(HKEY_USERS, sid_w + explorer, _T("NoLogoff"), !hasLogOff)) {
    allWorked = false;
    Tcout << _T("Error NoLogoff") << std::endl;
  }

  if(!WriteRegistryKey(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System"), _T("HideFastUserSwitching"), !hasSwitchUser)) {
    allWorked = false;
    Tcout << _T("Error HideFastUserSwitching") << std::endl;
  }

#ifndef DEBUG

  if(!WriteRegistryKey(HKEY_USERS, sid_w + system, _T("DisableTaskMgr"), !hasTaskManager)) {
    allWorked = false;
    Tcout << _T("Error DisableTaskMgr") << std::endl;
  }

#endif // DEBUG

  if(!WriteRegistryKey(HKEY_USERS, sid_w + explorer, _T("NoClose"), !hasPower)) {
    allWorked = false;
    Tcout << _T("Error NoClose") << std::endl;
  }

  if(!WriteFilterKeysHotkey(hasFilterKeysHotkey)) {
    allWorked = false;
    Tcout << _T("Error FilterKeysHotkey") << std::endl;
  }

  if(!WriteMouseKeysHotkey(hasMouseKeysHotkey)) {
    allWorked = false;
    Tcout << _T("Error MouseKeysHotkey") << std::endl;
  }

  if(!WriteStickyKeysHotkey(hasStickyKeysHotkey)) {
    allWorked = false;
    Tcout << _T("Error StickyKeysHotkey") << std::endl;
  }

  if(!WriteToggleKeysHotkey(hasToggleKeysHotkey)) {
    allWorked = false;
    Tcout << _T("Error ToggleKeysHotkey") << std::endl;
  }

  if(!WriteHighContrastHotkey(hasHighContrastHotkey)) {
    allWorked = false;
    Tcout << _T("Error HighContrastHotkey") << std::endl;
  }

 if(!WriteRegistryKey(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power"), _T("HiberbootEnabled"), hasFastBootHotKey)) {
    allWorked = false;
    Tcout << _T("Error FastBootHotKey") << std::endl;
  }

  return allWorked;
}

bool PersistentSystemOptions::ReadRegistryKey(HKEY baseHive, tstring path, tstring name, bool defaultValue)
{
  HKEY key;

  if(ERROR_SUCCESS != RegOpenKeyEx(baseHive, path.c_str(), 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY | STANDARD_RIGHTS_READ, &key))
  {
    return defaultValue;
  }

  DWORD type;
  DWORD value;
  DWORD valueSize = sizeof(value);

  if(ERROR_SUCCESS != RegQueryValueEx(key, name.c_str(), NULL, &type, (LPBYTE)&value, (LPDWORD)&valueSize))
  {
    RegCloseKey(key);
    return defaultValue;
  }

  RegCloseKey(key);

  if(type != REG_DWORD)
  {
    return defaultValue;
  }

  return value != 0;
}

bool PersistentSystemOptions::WriteRegistryKey(HKEY baseHive, tstring path, tstring name, bool value)
{
  HKEY key;

  if(ERROR_SUCCESS != RegCreateKeyEx(baseHive, path.c_str(), 0, NULL, REG_OPTION_VOLATILE, KEY_SET_VALUE | KEY_WOW64_64KEY | STANDARD_RIGHTS_WRITE | WRITE_OWNER, NULL, &key, NULL))
  {
    return false;
  }

  DWORD finalValue = value ? 1 : 0;

  if(ERROR_SUCCESS != RegSetValueEx(key, name.c_str(), 0, REG_DWORD, (LPCBYTE)&finalValue, sizeof(finalValue)))
  {
    RegCloseKey(key);
    return false;
  }

  RegCloseKey(key);
  return true;
}

bool PersistentSystemOptions::ReadFilterKeysHotkey()
{
  FILTERKEYS settings;
  settings.cbSize = sizeof(settings);
  if(!SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(settings), (PVOID)&settings, 0))
    return true;
  if(settings.dwFlags & FKF_HOTKEYACTIVE)
    return true;
  return false;
}

bool PersistentSystemOptions::WriteFilterKeysHotkey(bool value)
{
  FILTERKEYS settings;
  settings.cbSize = sizeof(settings);
  if(!SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(settings), (PVOID)&settings, 0))
    return false;
  const DWORD flag = FKF_HOTKEYACTIVE;
  settings.dwFlags &= ~flag;
  if(value)
    settings.dwFlags |= flag;
  if(!SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(settings), (PVOID)&settings, SPIF_SENDCHANGE))
    return false;
  return true;
}

bool PersistentSystemOptions::ReadMouseKeysHotkey()
{
  MOUSEKEYS settings;
  settings.cbSize = sizeof(settings);
  if(!SystemParametersInfo(SPI_GETMOUSEKEYS, sizeof(settings), (PVOID)&settings, 0))
    return true;
  if(settings.dwFlags & MKF_HOTKEYACTIVE)
    return true;
  return false;
}

bool PersistentSystemOptions::WriteMouseKeysHotkey(bool value)
{
  MOUSEKEYS settings;
  settings.cbSize = sizeof(settings);
  if(!SystemParametersInfo(SPI_GETMOUSEKEYS, sizeof(settings), (PVOID)&settings, 0))
    return false;
  const DWORD flag = MKF_HOTKEYACTIVE;
  settings.dwFlags &= ~flag;
  if(value)
    settings.dwFlags |= flag;
  if(!SystemParametersInfo(SPI_SETMOUSEKEYS, sizeof(settings), (PVOID)&settings, SPIF_SENDCHANGE))
    return false;
  return true;
}

bool PersistentSystemOptions::ReadStickyKeysHotkey()
{
  STICKYKEYS settings;
  settings.cbSize = sizeof(settings);
  if(!SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(settings), (PVOID)&settings, 0))
    return true;
  if(settings.dwFlags & SKF_HOTKEYACTIVE)
    return true;
  return false;
}

bool PersistentSystemOptions::WriteStickyKeysHotkey(bool value)
{
  STICKYKEYS settings;
  settings.cbSize = sizeof(settings);
  if(!SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(settings), (PVOID)&settings, 0))
    return false;
  const DWORD flag = SKF_HOTKEYACTIVE;
  settings.dwFlags &= ~flag;
  if(value)
    settings.dwFlags |= flag;
  if(!SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(settings), (PVOID)&settings, SPIF_SENDCHANGE))
    return false;
  return true;
}

bool PersistentSystemOptions::ReadToggleKeysHotkey()
{
  TOGGLEKEYS settings;
  settings.cbSize = sizeof(settings);
  if(!SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(settings), (PVOID)&settings, 0))
    return true;
  if(settings.dwFlags & TKF_HOTKEYACTIVE)
    return true;
  return false;
}

bool PersistentSystemOptions::WriteToggleKeysHotkey(bool value)
{
  TOGGLEKEYS settings;
  settings.cbSize = sizeof(settings);
  if(!SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(settings), (PVOID)&settings, 0))
    return false;
  const DWORD flag = TKF_HOTKEYACTIVE;
  settings.dwFlags &= ~flag;
  if(value)
    settings.dwFlags |= flag;
  if(!SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(settings), (PVOID)&settings, SPIF_SENDCHANGE))
    return false;
  return true;
}

bool PersistentSystemOptions::ReadHighContrastHotkey()
{
  HIGHCONTRAST settings;
  settings.cbSize = sizeof(settings);
  if(!SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(settings), (PVOID)&settings, 0))
    return true;
  if(settings.dwFlags & HCF_HOTKEYACTIVE)
    return true;
  return false;
}

bool PersistentSystemOptions::WriteHighContrastHotkey(bool value)
{
  HIGHCONTRAST settings;
  settings.cbSize = sizeof(settings);
  if(!SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(settings), (PVOID)&settings, 0))
    return false;
  const DWORD flag = HCF_HOTKEYACTIVE;
  settings.dwFlags &= ~flag;
  if(value)
    settings.dwFlags |= flag;
  if(!SystemParametersInfo(SPI_SETHIGHCONTRAST, sizeof(settings), (PVOID)&settings, SPIF_SENDCHANGE))
    return false;
  return true;
}


PersistentSystemOptions PersistentSystemOptions::ReadFromStorage()
{
  WCHAR *sid_w = getSid();

    std::wstring sid_ws(sid_w);
// your new String
    std::string sid_str(sid_ws.begin(), sid_ws.end());
   // const char*sid=sid_str.c_str();

  std::string options = "\\PersistentSystemOptions";
  std::string sep = "\\";
  std::string dest_str = sid_str + sep + REGISTRY_SETTINGS_LOCATION + options;
    std::wstring dest=s2ws(dest_str);

  PersistentSystemOptions retval;
  retval.hasChangePassword = ReadRegistryKey(HKEY_USERS, dest, _T("hasChangePassword"), true);
  retval.hasLockComputer = ReadRegistryKey(HKEY_USERS, dest, _T("hasLockComputer"), true);
  retval.hasLogOff = ReadRegistryKey(HKEY_USERS, dest, _T("hasLogOff"), true);
  retval.hasSwitchUser = ReadRegistryKey(HKEY_USERS, dest, _T("hasSwitchUser"), true);
  retval.hasTaskManager = ReadRegistryKey(HKEY_USERS, dest, _T("hasTaskManager"), true);
  retval.hasPower = ReadRegistryKey(HKEY_USERS, dest, _T("hasPower"), true);
  retval.hasFilterKeysHotkey = ReadRegistryKey(HKEY_USERS, dest, _T("hasFilterKeysHotkey"), true);
  retval.hasMouseKeysHotkey = ReadRegistryKey(HKEY_USERS, dest, _T("hasMouseKeysHotkey"), true);
  retval.hasStickyKeysHotkey = ReadRegistryKey(HKEY_USERS, dest, _T("hasStickyKeysHotkey"), true);
  retval.hasToggleKeysHotkey = ReadRegistryKey(HKEY_USERS, dest, _T("hasToggleKeysHotkey"), true);
  retval.hasHighContrastHotkey = ReadRegistryKey(HKEY_USERS, dest, _T("hasHighContrastHotkey"), true);
  retval.hasFastBootHotKey = ReadRegistryKey(HKEY_USERS, dest, _T("hasFastBootHotkey"), true);
  return retval;
}

bool PersistentSystemOptions::WriteToStorage() const
{
  WCHAR* sid_w = getSid();
    std::wstring sid_ws(sid_w);
// your new String
   std::string sid_str(sid_ws.begin(), sid_ws.end());

  std::string options = "\\PersistentSystemOptions";
  std::string sep = "\\";
  std::string dest_str = sid_str + sep + REGISTRY_SETTINGS_LOCATION + options;
  std::wstring dest=s2ws(dest_str);

  bool allWorked = true;

  if(!WriteRegistryKey(HKEY_USERS, dest, _T("hasChangePassword"), hasChangePassword))
    allWorked = false;

  if(!WriteRegistryKey(HKEY_USERS, dest, _T("hasLockComputer"), hasLockComputer))
    allWorked = false;

  if(!WriteRegistryKey(HKEY_USERS, dest, _T("hasLogOff"), hasLogOff))
    allWorked = false;

  if(!WriteRegistryKey(HKEY_USERS, dest, _T("hasSwitchUser"), hasSwitchUser))
    allWorked = false;

  if(!WriteRegistryKey(HKEY_USERS, dest, _T("hasTaskManager"), hasTaskManager))
    allWorked = false;

  if(!WriteRegistryKey(HKEY_USERS, dest, _T("hasPower"), hasPower))
    allWorked = false;

  if(!WriteRegistryKey(HKEY_USERS, dest, _T("hasFilterKeysHotkey"), hasFilterKeysHotkey))
    allWorked = false;

  if(!WriteRegistryKey(HKEY_USERS, dest, _T("hasMouseKeysHotkey"), hasMouseKeysHotkey))
    allWorked = false;

  if(!WriteRegistryKey(HKEY_USERS, dest, _T("hasStickyKeysHotkey"), hasStickyKeysHotkey))
    allWorked = false;

  if(!WriteRegistryKey(HKEY_USERS, dest, _T("hasToggleKeysHotkey"), hasToggleKeysHotkey))
    allWorked = false;

  if(!WriteRegistryKey(HKEY_USERS, dest, _T("hasHighContrastHotkey"), hasHighContrastHotkey))
    allWorked = false;

   if(!WriteRegistryKey(HKEY_USERS, dest, _T("hasFastBootHotkey"), !hasFastBootHotKey))
    allWorked = false;

  return allWorked;
}

PersistentSystemOptions persistentSystemOptions;

int action_block() {
	//FindWindow() need two wchar_t * as args. Compiler make error if you put char* instead
    std::wstring arg1_string( L"Shell_TrayWnd");
    const wchar_t* arg1 = arg1_string.c_str();

    std::wstring arg2_string( L"");
    const wchar_t* arg2 = arg2_string.c_str();

  ShowWindow(FindWindow(arg1,arg2), SW_HIDE);
  bool succeded = PersistentSystemOptions::AllDisabled().WriteSystemSettings();
  if(verbose)
  {
    if(succeded)
      Tcout << _T("Disabled persistent system options.") << std::endl;
    else
      Tcout << _T("Reseting persistent system options failed.") << std::endl;
  }
  if(!succeded)
    return 1;
  return 0;
}

int action_unblock() {
	//FindWindow() need two wchar_t * as args. Compiler make error if you put char* instead
    std::wstring arg1_string( L"Shell_TrayWnd");
    const wchar_t* arg1 = arg1_string.c_str();

    std::wstring arg2_string( L"");
    const wchar_t* arg2 = arg2_string.c_str();

  ShowWindow(FindWindow(arg1,arg2), SW_SHOW);
  bool succeded = PersistentSystemOptions::AllEnabled().WriteSystemSettings();
  if(verbose)
  {
    if(succeded)
      Tcout << _T("Disabled persistent system options.") << std::endl;
    else
      Tcout << _T("Reseting persistent system options failed.") << std::endl;
  }
  if(!succeded)
    return 1;
  return 0;
}

int action_dump() {
  persistentSystemOptions = PersistentSystemOptions::ReadSystemSettings();
  persistentSystemOptions.Dump(Tcout);
  return 0;
}

int action_force_unblock() {
	//FindWindow() need two wchar_t * as args. Compiler make error if you put char* instead
    std::wstring arg1_string( L"Shell_TrayWnd");
    const wchar_t* arg1 = arg1_string.c_str();

    std::wstring arg2_string( L"");
    const wchar_t* arg2 = arg2_string.c_str();

  ShowWindow(FindWindow(arg1,arg2), SW_SHOW);

  bool succeded = PersistentSystemOptions::AllEnabled().WriteSystemSettings();
  if(verbose)
  {
    if(succeded)
      Tcout << _T("Reset persistent system options.") << std::endl;
    else
      Tcout << _T("Reseting persistent system options failed.") << std::endl;
  }
  if(!succeded)
    return 1;
  return 0;
}

int action_save() {
  persistentSystemOptions = PersistentSystemOptions::ReadSystemSettings();
  if(verbose)
  {
    Tcout << _T("Read persistent system options:") << std::endl;
    persistentSystemOptions.Dump(Tcout);
  }
  bool succeded = persistentSystemOptions.WriteToStorage();
  if(verbose)
  {
    if(succeded)
      Tcout << _T("Saved persistent system options.") << std::endl;
    else
      Tcout << _T("Saving persistent system options failed.") << std::endl;
  }
  if(!succeded)
    return 1;
  return 0;
}

int action_write_settings() {
  if(!persistentSystemOptions.WriteSystemSettings())
  {
    if(verbose)
    {
      Tcout << _T("Writing persistent system options failed.");
    }
    return 1;
  }
  return 0;
}
