/* Copyright (c) 2015 Jacob Lifshay, Fork Ltd.
 * This file is part of Prey.
 *
 * Prey is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Prey is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Prey.  If not, see <http://www.gnu.org/licenses/>.
 */
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
#include <propsys.h>
#include <propkey.h>
#include <shellapi.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <sstream>
#include <cstdint>
#include <cctype>
#include <iostream>
#include <initializer_list>
#include "resources.h"
#include "persistent.h"
#include "md5.h"
#include "base64.h"
#include "string_cast.h"
#include "get_option.h"
#include <ctime>


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

  template <typename Fn>
BOOL CALLBACK EnumDisplayMonitorsHelper(HMONITOR monitor, HDC monitorDC, LPRECT pMonitorRect, LPARAM arg)
{
  Fn &f = *(Fn *)arg;
  return f(monitor, monitorDC, pMonitorRect);
}

  template <typename Fn>
BOOL EnumDisplayMonitors(HDC dc, LPCRECT pClipRect, Fn fn)
{
  return ::EnumDisplayMonitors(dc, pClipRect, &EnumDisplayMonitorsHelper<Fn>, (LPARAM)std::addressof(fn));
}

  template <typename Fn>
BOOL CALLBACK EnumWindowsHelper(HWND window, LPARAM arg)
{
  Fn &f = *(Fn *)arg;
  return f(window);
}

  template <typename Fn>
BOOL EnumWindows(Fn fn)
{
  return ::EnumWindows(&EnumWindowsHelper<Fn>, (LPARAM)std::addressof(fn));
}

class MD5 final
{
  public:
    struct Hash final
    {
      static constexpr std::size_t dataSize = 16;
      std::uint8_t data[dataSize];
      static bool parse(tstring str, Hash &retval)
      {
        str.erase(std::remove(str.begin(), str.end(), '"'), str.end());
        if(str.size() != 2 * dataSize)
        {
            return false;
        }



        for(std::size_t i = 0; i < dataSize; i++)
        {
          if((char)str[i * 2] != str[i * 2] || !std::isxdigit((char)str[i * 2]))
          {
            return false;
          }

          if((char)str[i * 2 + 1] != str[i * 2 + 1] || !std::isxdigit((char)str[i * 2 + 1]))
          {
            return false;
          }

          tistringstream ss(str.substr(i * 2, 2));
          int v;
          ss >> std::hex >> v;
          retval.data[i] = v;
        }

        return true;
      }
      bool operator ==(const Hash &rt) const
      {
        for(std::size_t i = 0; i < dataSize; i++)
        {
          if(data[i] != rt.data[i])
          {
            return false;
          }
        }

        return true;
      }
      bool operator !=(const Hash &rt) const
      {
        return !operator ==(rt);
      }
      friend std::ostream &operator <<(std::ostream &os, const Hash &v)
      {
        std::ostringstream ss;
        ss << std::hex;
        for(std::size_t i = 0; i < dataSize; i++)
        {
          ss << (int)v.data[i] / 0x10 << (int)v.data[i] % 0x10;
        }
        return os << ss.str();
      }
      friend std::wostream &operator <<(std::wostream &os, const Hash &v)
      {
        std::wostringstream ss;
        ss << std::hex;
        for(std::size_t i = 0; i < dataSize; i++)
        {
          ss << (int)v.data[i] / 0x10 << (int)v.data[i] % 0x10;
        }
        return os << ss.str();
      }
    };
    static MD5 make()
    {
      return MD5();
    }
    MD5 &append(const std::uint8_t *bytes, std::size_t length) &
    {
      if(length == 0)
        return *this;

      const std::size_t maxChunkSize = (std::size_t)INT_MAX / 2;

      while(length > maxChunkSize)
      {
        md5_append(&state, bytes, maxChunkSize);
        length -= maxChunkSize;
        bytes += maxChunkSize;
      }

      md5_append(&state, bytes, length);
      return *this;
    }
    MD5 &&append(const std::uint8_t *bytes, std::size_t length) &&
    {
      MD5 &v = *this;
      return std::move(v.append(bytes, length));
    }
    Hash finish()
    {
      Hash retval;
      md5_finish(&state, retval.data);
      return retval;
    }
    static Hash hash(const std::uint8_t *bytes, std::size_t length)
    {
      return make().append(bytes, length).finish();
    }
  private:
    MD5()
    {
      md5_init(&state);
    }
    md5_state_t state;
};

MD5::Hash hashPassword(tstring password)
{
  std::string utf8password = string_cast<std::string>(password);
  std::string b64_encoded = base64_encode(reinterpret_cast<const unsigned char*>(utf8password.c_str()), utf8password.length());
  return MD5::hash((const std::uint8_t *)b64_encoded.data(), b64_encoded.size());
}

struct Window final
{
  HWND window;
  bool isPrimary;
  Window(bool isPrimary = false)
    : window(NULL),
    isPrimary(isPrimary)
  {
  }
};

class Application final
{
  private:
    struct CreateData final
    {
      Window initWindow;
      Application *application;
    };
    static LRESULT CALLBACK StaticWindowProcedure(HWND window, UINT messageId, WPARAM wParam, LPARAM lParam);
    LRESULT WindowProcedure(Window &window, UINT messageId, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK StaticKeyboardHookProcedure(int nCode, WPARAM wParam, LPARAM lParam);
    static HRESULT EnableWindowSystemTouchGestures(HWND window, bool enabled);
    void EnableTouchKeyboard(Window &parentWindow, bool enabled);
    bool IsWow64Process();
    bool ParseArgumentsAndCheckForHelpOption();
    const LPCTSTR className = _T("MyClass");
    const HINSTANCE instance;
    const int nCmdShow;
    std::unordered_map<HWND, Window> windows;
    std::unordered_map<HMONITOR, HWND> monitors;
    tstring textMessage;
    HWND passwordControl;
    HWND passwordControl2;
    HWND unlockControl;
    HWND startKeyboardControl;
    HWND backgroundImageControl;
    static constexpr UINT passwordControlId = 100;
    static constexpr UINT unlockControlId = 101;
    static constexpr UINT startKeyboardControlId = 102;
    static constexpr UINT backgroundImageControlId = 103;
    HHOOK keyboardHook;
    HICON keyboardIcon;
    MD5::Hash hashedPassword;
    bool debugPassword = false;
    tstring commandLine;
    HBITMAP backgroundImage;
    HBITMAP backgroundImageError;
    std::vector<tstring> args;
    bool verbose = false;
    bool primaryCreated = false;
    enum class Action
    {
      LockScreen,
      Default = LockScreen,
      Block,
      Unblock,
      Save,
      ForceUnblock,
      Version,
      Dump,
    };
  public:
    Application(HINSTANCE instance, tstring commandLine, int nCmdShow)
      : instance(instance),
      nCmdShow(nCmdShow),
      commandLine(commandLine)
  {
  }
    int operator()();
};

int WINAPI _tWinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPTSTR lpszArgument, int nCmdShow)
{
  return Application(hThisInstance, lpszArgument, nCmdShow)();
}

bool Application::ParseArgumentsAndCheckForHelpOption()
{
  tstring programName;
  programName.reserve(128);
  programName.resize(64);
  DWORD programNameSize = programName.size();

  do
  {
    programNameSize = GetModuleFileName(NULL, &programName[0], programName.size());

    if(!programNameSize)
    {
      break;
    }

    programName.resize(programName.size() * 2);
  }
  while(programNameSize + 1 >= programName.size());

  programName.resize(programNameSize);
  args.assign(1, programName);
  bool inQuotedString = false;
  bool gotAnything = false;
  bool gotOptionSlash = false;
  bool isHelpOption = false;
  bool startNewArg = true;
  std::size_t backslashCount = 0;
  auto parseChar = [&](_TINT ch)->void
  {
    if(ch == _T('\\'))
    {
      gotAnything = true;
      gotOptionSlash = false;
      isHelpOption = false;
      backslashCount++;
      return;
    }

    if(ch == _T('\"'))
    {
      gotAnything = true;
      gotOptionSlash = false;
      isHelpOption = false;

      if(startNewArg)
      {
        startNewArg = false;
        args.emplace_back();
      }

      if(backslashCount % 2 != 0)
      {
        args.back().append(backslashCount / 2, _T('\\'));
        backslashCount = 0;
        args.back() += _T('\"');
        return;
      }

      args.back().append(backslashCount / 2, _T('\\'));
      backslashCount = 0;
      inQuotedString = !inQuotedString;
    }

    if(backslashCount > 0)
    {
      if(startNewArg)
      {
        startNewArg = false;
        args.emplace_back();
      }

      args.back().append(backslashCount, _T('\\'));
      backslashCount = 0;
    }

    if(ch == _TEOF)
      return;

    if(inQuotedString)
    {
      args.back() += ch;
      return;
    }

    if(!gotAnything)
    {
      if(ch == _T(' ') || ch == _T('\t'))
      {
        return;
      }

      gotAnything = true;

      if(startNewArg)
      {
        startNewArg = false;
        args.emplace_back();
      }

      args.back() += ch;

      if(ch == _T('/'))
        gotOptionSlash = true;

      return;
    }

    if(gotOptionSlash && ch == _T('?'))
    {
      gotOptionSlash = false;

      if(startNewArg)
      {
        startNewArg = false;
        args.emplace_back();
      }

      args.back() += ch;
      isHelpOption = true;
      return;
    }

    if(ch == _T(' ') || ch == _T('\t'))
    {
      startNewArg = true;
      return;
    }

    gotOptionSlash = false;
    isHelpOption = false;

    if(startNewArg)
    {
      startNewArg = false;
      args.emplace_back();
    }

    args.back() += ch;
  };

  for(TCHAR ch : commandLine)
  {
    parseChar(ch);
  }

  parseChar(_TEOF);

  return isHelpOption;
}

int Application::operator()()
{
  tstring password;

  bool isHelpOption = ParseArgumentsAndCheckForHelpOption();

  Action action = Action::LockScreen;
  std::size_t actionSpecificationCount = 0;
  tstring dumpFileName = _T("-");
  bool writeHelpToStdErr = false;
  bool tooManyArguments = false;

  if(!isHelpOption)
  {
    const std::vector<tget_option::option> options = std::initializer_list<tget_option::option>
    {
      tget_option::option{_T("help"), tget_option::argument::no, nullptr, _T('h')},
        tget_option::option{_T("block"), tget_option::argument::no, nullptr, _T('b')},
        tget_option::option{_T("unblock"), tget_option::argument::no, nullptr, _T('u')},
        tget_option::option{_T("save"), tget_option::argument::no, nullptr, _T('s')},
        tget_option::option{_T("force-unblock"), tget_option::argument::no, nullptr, _T('U')},
        tget_option::option{_T("version"), tget_option::argument::no, nullptr, _T('V')},
        tget_option::option{_T("verbose"), tget_option::argument::no, nullptr, _T('v')},
        tget_option::option{_T("dump"), tget_option::argument::optional, nullptr, _T('d')},
    };
    _TINT opt;
    tget_option optionParser([&](tstring msg)
        {
        Tcerr << _T("lock-screen: ") << msg << std::endl;
        writeHelpToStdErr = true;
        });

    while((opt = optionParser.getopt_long(args, _T("hbusUvd::"), options, nullptr)) != optionParser.end_flag())
    {
      switch(opt)
      {
        case _T('h'):
          isHelpOption = true;
          break;

        case _T('b'):
          action = Action::Block;
          actionSpecificationCount++;
          break;

        case _T('u'):
          action = Action::Unblock;
          actionSpecificationCount++;
          break;

        case _T('s'):
          action = Action::Save;
          actionSpecificationCount++;
          break;

        case _T('U'):
          action = Action::ForceUnblock;
          actionSpecificationCount++;
          break;

        case _T('V'):
          action = Action::Version;
          actionSpecificationCount++;
          break;

        case _T('d'):
          action = Action::Dump;
          actionSpecificationCount++;

          if(optionParser.has_arg)
          {
            dumpFileName = optionParser.optarg;
          }

          break;

        case _T('v'):
          verbose = true;
          break;

        default:
          isHelpOption = true;
          writeHelpToStdErr = true;
          break;
      }
    }

    std::size_t argumentsLeft = args.size() - optionParser.optind;

    /*
    if(actionSpecificationCount > 0)
    {
      if(argumentsLeft > 0)
      {
        tooManyArguments = true;
      }
    }
    else if(argumentsLeft > 1)
    {
      tooManyArguments = true;
    }
    else if(argumentsLeft == 1)
    {
      password = args[optionParser.optind];

    }
    */
    if (argumentsLeft > 1) {
      password = args[1];
      textMessage = args[2];
    }
    else if(argumentsLeft == 1) {
      password = args[1];
      textMessage = _T("");
    }

  }

  if(!isHelpOption)
  {
    if(actionSpecificationCount > 1)
    {
      Tcerr << _T("lock-screen: multiple actions specified") << std::endl;
      isHelpOption = true;
      writeHelpToStdErr = true;
    }
    else if(tooManyArguments)
    {
      Tcerr << _T("lock-screen: too many arguments") << std::endl;
      isHelpOption = true;
      writeHelpToStdErr = true;
    }
  }

  if(isHelpOption)
  {
    const LPCTSTR helpMessage = _T(""
        "Usage:\tlock-screen [<options>]\n"
        "\tlock-screen <password>\n"
        "\tlock-screen <MD5-hashed password>\n"
        "\n"
        "Action-Options: \n"
        "-b,--block\t\t\tSave, then block the\n"
        "\t\t\t\tpersistent lock-screen settings.\n"
        "\n"
        "-d,--dump [<output file>]\tDisplay the persistent lock-screen settings.\n"
        "\n"
        "-s,--save\t\t\tSave the persistent lock-screen settings.\n"
        "\n"
        "-u,--unblock\t\t\tRestore the persistent lock-screen settings.\n"
        "\n"
        "-U,--force-unblock\t\tReset the persistent lock-screen settings.\n"
        "\n"
        "--version\t\t\tDisplay version information.\n"
        "\n"
        "Options: \n"
        "-h,--help,/?\t\t\tShow this help.\n"
        "\n"
        "-v,--verbose\t\t\tEnable verbosity.\n");
    if(writeHelpToStdErr)
    {
      Tcerr << helpMessage;
      return 1;
    }
    Tcout << helpMessage;
    return 0;
  }

  switch(action)
  {
    case Action::LockScreen:
      break;
    case Action::Block:
      {
        if(action_block()) {
          return 0;
        }
        return 1;
      }
    case Action::Dump:
      {
        if(action_dump()) {
          return 0;
        }
        return 1;
      }
    case Action::ForceUnblock:
      {
        if(action_force_unblock()) {
          return 0;
        }
        return 1;
      }
    case Action::Save:
      {
        if (action_save()) {
          return 0;
        }
        return 1;
      }
    case Action::Unblock:
      {
        if(action_unblock()) {
          return 0;
        } else {
          return 1;
        }
      }
    case Action::Version:
      {
        Tcout << _T(VERSION_STRING) << std::endl;
        return 0;
      }
  }

  if(password.empty())
  {
    debugPassword = true;
    hashedPassword = hashPassword(_T("exit"));
    if(verbose)
    {
      Tcout << _T("Using debug password: \"exit\"") << std::endl;
    }
  }
  else if(!MD5::Hash::parse(password, hashedPassword))
  {
    hashedPassword = hashPassword(password);
  }

  if(verbose)
  {
    Tcout << _T("Hashed password: ") << hashedPassword << std::endl;
  }

  password = _T("");

  CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  keyboardIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_KEYBOARD));
  backgroundImage = (HBITMAP)LoadImage(instance, MAKEINTRESOURCE(IDB_BACKGROUND), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
  backgroundImageError = (HBITMAP)LoadImage(instance, MAKEINTRESOURCE(IDB_BACKGROUND_ERROR), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);

  WNDCLASSEX windowClass;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = className;
  windowClass.lpfnWndProc = StaticWindowProcedure;
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_MAIN_ICON));
  windowClass.hIconSm = windowClass.hIcon;
  windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
  windowClass.lpszMenuName = NULL;
  windowClass.cbClsExtra = 0;
  windowClass.cbWndExtra = 0;
  windowClass.hbrBackground = (HBRUSH)COLOR_BACKGROUND;

  if(!RegisterClassEx(&windowClass))
  {
    return 1;
  }

  action_dump();
  action_save();
  action_block();

  DWORD sessId = WTSGetActiveConsoleSessionId();

  while (sessId == 0)
  {
    sessId = WTSGetActiveConsoleSessionId();
  }
  EnumDisplayMonitors(NULL, NULL, [this](HMONITOR monitor, HDC monitorDC, LPRECT pMonitorRect)->BOOL
      {
      MONITORINFOEX monitorInfo;
      monitorInfo.cbSize = sizeof(monitorInfo);
      GetMonitorInfo(monitor, &monitorInfo);
      CreateData createData;
      createData.initWindow = Window((monitorInfo.dwFlags & MONITORINFOF_PRIMARY) == MONITORINFOF_PRIMARY);
      createData.application = this;
      HWND handle = CreateWindowEx(
#ifdef DEBUG
          0,
#else
          WS_EX_TOPMOST,
#endif
          className,
          _T("Lock Screen"),
          WS_POPUP,
          pMonitorRect->left,
          pMonitorRect->top,
          pMonitorRect->right - pMonitorRect->left,
          pMonitorRect->bottom - pMonitorRect->top,
          HWND_DESKTOP,
          NULL,
          instance,
          (LPVOID)&createData);
      monitors.emplace(monitor, handle);
      if(verbose)
      {
        Tcout << _T("Created window for monitor: ") << monitorInfo.szDevice << _T(" (") << pMonitorRect->left << _T(", ") << pMonitorRect->top << _T(")-(") << pMonitorRect->right << _T(", ") << pMonitorRect->bottom << _T(")") << std::endl;
      }
      return TRUE;
      });

  primaryCreated = true;
  /* Make the window visible on the screen */
  for(auto i : windows)
  {
    ShowWindow(std::get<1>(i).window, nCmdShow);
  }

  SetFocus(passwordControl);

  keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, StaticKeyboardHookProcedure, NULL, 0);
  if(verbose && keyboardHook != NULL)
  {
    Tcout << _T("Installed keyboard hook.") << std::endl;
  }
  else if(verbose)
  {
    Tcout << _T("Keyboard hook installation failed.") << std::endl;
  }

  /* Run the message loop. It will run until GetMessage() returns 0 */
  MSG message;
  std::time_t time = std::time(0);
  INPUT Inputs[3] = {0};

  Inputs[0].type = INPUT_MOUSE;
  Inputs[0].mi.dx = 0; // desired X coordinate
  Inputs[0].mi.dy = 0; // desired Y coordinate
  Inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

  Inputs[1].type = INPUT_MOUSE;
  Inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

  Inputs[2].type = INPUT_MOUSE;
  Inputs[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;
  SetFocus(passwordControl);
  SendInput(3, Inputs, sizeof(INPUT));
  for(BOOL retval = GetMessage(&message, NULL, 0, 0); retval != 0; retval = GetMessage(&message, NULL, 0, 0))
  {
    if(difftime(std::time(0), time) >= 10){
      std::cout << "SET FOCUS!!!!\n";
      SendInput(3, Inputs, sizeof(INPUT));
      SetFocus(passwordControl);
      time = std::time(0);
    }
    if(retval == -1)
    {
      return 1;
    }

    bool handled = false;

    for(auto i : windows)
    {
      if(IsDialogMessage(std::get<1>(i).window, &message))
      {
        handled = true;
        break;
      }
    }

    if(handled)
    {
      continue;
    }

    /* Translate virtual-key messages into character messages */
    TranslateMessage(&message);
    /* Send message to WindowProcedure */
    DispatchMessage(&message);
  }

  if(keyboardHook)
  {
    UnhookWindowsHookEx(keyboardHook);
    if(verbose)
    {
      Tcout << _T("Removed keyboard hook.") << std::endl;
    }
  }
  action_unblock();

  /* The program return-value is the value that we gave PostQuitMessage() */
  return message.wParam;
}

//The function responsible for detecting the keystrokes
LRESULT CALLBACK Application::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
  if (nCode < 0)
      return CallNextHookEx(NULL, nCode, wParam, lParam);

  tagKBDLLHOOKSTRUCT *str = (tagKBDLLHOOKSTRUCT *)lParam;

  switch(str->flags)
  {
      case (LLKHF_ALTDOWN):
          delete str;
      return 1;
  }

  if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN || wParam == WM_HOTKEY)
  {
      switch (str->vkCode)
      {
          case VK_RWIN:
          case VK_LWIN:
          case VK_LCONTROL:
          case VK_TAB:
          case VK_RCONTROL:
          case VK_APPS:
          case VK_SLEEP:
          case VK_MENU:
              delete str;
          return 1;
      }
  }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK Application::StaticWindowProcedure(HWND hwnd, UINT messageId, WPARAM wParam, LPARAM lParam)
{
  Application *application = (Application *)GetWindowLongPtr(hwnd, GWLP_USERDATA);


  if(application)
  {
    if (application->windows.find(hwnd) == application->windows.end())
    {
        LPCREATESTRUCT createStruct = (LPCREATESTRUCT)lParam;
        CreateData *createData = (CreateData *)createStruct->lpCreateParams;
        application->windows.emplace(hwnd, createData->initWindow);
    }
    Window &window = application->windows.at(hwnd);
    return application->WindowProcedure(window, messageId, wParam, lParam);
  }

  if(messageId == WM_NCCREATE || messageId == WM_CREATE)
  {
    LPCREATESTRUCT createStruct = (LPCREATESTRUCT)lParam;
    CreateData *createData = (CreateData *)createStruct->lpCreateParams;
    application = createData->application;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)application);
    createData->initWindow.window = hwnd;
    return application->WindowProcedure(std::get<1>(*std::get<0>(application->windows.emplace(hwnd, createData->initWindow))), messageId, wParam, lParam);
  }

  return DefWindowProc(hwnd, messageId, wParam, lParam);
}

/* This function is called by the Windows function DispatchMessage() */
LRESULT Application::WindowProcedure(Window &window, UINT messageId, WPARAM wParam, LPARAM lParam)
{
  switch(messageId)  /* handle the messages */
  {
    case WM_CREATE:
      {
        if(window.isPrimary)
        {
          RECT windowRect;
          GetClientRect(window.window, &windowRect);
          POINT windowCenter;
          windowCenter.x = (windowRect.right - windowRect.left) / 2;
          windowCenter.y = (windowRect.bottom - windowRect.top) / 2;
          backgroundImageControl = CreateWindowEx(0,
              _T("STATIC"),
              NULL,
              WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE,
              windowCenter.x - 162,
              windowCenter.y - 253,
              324,
              374,
              window.window,
              (HMENU)backgroundImageControlId,
              instance,
              NULL);
          SendMessage(backgroundImageControl, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)backgroundImage);
          SIZE editSize = {290, 30};
          RECT editRect;
          editRect.left = windowCenter.x - editSize.cx / 2;
          editRect.right = editRect.left + editSize.cx;
          editRect.top = windowCenter.y - editSize.cy / 2;
          editRect.bottom = editRect.top + editSize.cy;
          passwordControl = CreateWindowEx(0,
              _T("EDIT"),
              _T(""),
              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_PASSWORD,
              editRect.left,
              editRect.top,
              editRect.right - editRect.left,
              editRect.bottom - editRect.top,
              window.window,
              (HMENU)passwordControlId,
              instance,
              NULL);

          textMessage.erase(remove( textMessage.begin(), textMessage.end(), '\"' ),textMessage.end());
          std::wstring stemp = std::wstring(textMessage.begin(), textMessage.end());

          LPCWSTR sw = stemp.c_str();
          SIZE editSize2 = {400, 200};

          RECT editRect2;
          editRect2.left = windowCenter.x - 150 - editSize2.cx / 2;
          editRect2.right = editRect.left + 100 + editSize2.cx;
          editRect2.top = windowCenter.y + 160 - editSize2.cy / 2;
          editRect2.bottom = editRect.top + 100 + editSize2.cy;
          passwordControl2 = CreateWindowEx(0,
              _T("STATIC"),
              sw,
              WS_CHILD | WS_VISIBLE | WS_TABSTOP | SS_CENTER,
              editRect2.left,
              editRect2.top,
              editRect2.right - editRect2.left,
              editRect2.bottom - editRect2.top,
              window.window,
              (HMENU)passwordControlId,
              instance,
              NULL);

          LOGFONT passwordControlFontDescriptor;
          GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(passwordControlFontDescriptor), &passwordControlFontDescriptor);
          passwordControlFontDescriptor.lfWidth = 0;
          HDC passwordControlDC = GetDC(passwordControl);
          passwordControlFontDescriptor.lfHeight = 30;
          ReleaseDC(passwordControl, passwordControlDC);
          HFONT passwordControlFont = CreateFontIndirect(&passwordControlFontDescriptor);

          if(passwordControlFont != NULL)
          {
            SendMessage(passwordControl, WM_SETFONT, (WPARAM)passwordControlFont, MAKELPARAM(FALSE, 0));
          }

          SendMessage(passwordControl, WM_SETTEXT, (WPARAM)NULL, (LPARAM)_T(""));
          SendMessage(passwordControl, EM_SETPASSWORDCHAR, (WPARAM)_T('●'), (LPARAM)NULL);
          RECT unlockRect = editRect;
          unlockRect.left = editRect.right + 5;
          unlockRect.right = unlockRect.left + 50;
          unlockControl = CreateWindowEx(0,
              _T("BUTTON"),
              _T("Unlock"),
              WS_CHILD | WS_TABSTOP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | BS_DEFPUSHBUTTON | BS_PUSHBUTTON | BS_OWNERDRAW,
              unlockRect.left,
              unlockRect.top,
              unlockRect.right - unlockRect.left,
              unlockRect.bottom - unlockRect.top,
              window.window,
              (HMENU)unlockControlId,
              instance,
              NULL);
          RECT startKeyboardRect;
          startKeyboardRect.right = windowRect.right - 5;
          startKeyboardRect.bottom = windowRect.bottom - 5;
          startKeyboardRect.top = startKeyboardRect.bottom - 64;
          startKeyboardRect.left = startKeyboardRect.right - 64;
          startKeyboardControl = CreateWindowEx(0,
              _T("BUTTON"),
              _T("On-Screen Keyboard"),
              WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | BS_OWNERDRAW,
              startKeyboardRect.left,
              startKeyboardRect.top,
              startKeyboardRect.right - startKeyboardRect.left,
              startKeyboardRect.bottom - startKeyboardRect.top,
              window.window,
              (HMENU)startKeyboardControlId,
              instance,
              NULL);
        }

        EnableWindowSystemTouchGestures(window.window, false);

        break;
      }

    case WM_DESTROY:
      PostQuitMessage(66);  /* send a WM_QUIT to the message queue */
      EnableTouchKeyboard(window, false);
      break;
    case WM_DISPLAYCHANGE:
    {
      EnumDisplayMonitors(NULL, NULL, [this, lParam, window ](HMONITOR monitor, HDC monitorDC, LPRECT pMonitorRect)->BOOL
          {
            if (monitors.find(monitor) == monitors.end())
            {
              MONITORINFOEX monitorInfo;
              monitorInfo.cbSize = sizeof(monitorInfo);
              GetMonitorInfo(monitor, &monitorInfo);
              if ((monitorInfo.dwFlags & MONITORINFOF_PRIMARY) != MONITORINFOF_PRIMARY)
              {
                  CreateData createData;
                  createData.initWindow = Window((monitorInfo.dwFlags & MONITORINFOF_PRIMARY) == MONITORINFOF_PRIMARY);
                  createData.application = this;
                  HWND handle = CreateWindowEx(
                #ifdef DEBUG
                      0,
                #else
                      WS_EX_TOPMOST,
                #endif
                      className,
                      _T("Lock Screen"),
                      WS_POPUP,
                      pMonitorRect->left,
                      pMonitorRect->top,
                      pMonitorRect->right - pMonitorRect->left,
                      pMonitorRect->bottom - pMonitorRect->top,
                      HWND_DESKTOP,
                      NULL,
                      instance,
                      (LPVOID)&createData);
                  if (handle == NULL)
                  {
                      wchar_t err[256];
                      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
                                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), err, 255, NULL);
                      Tcerr << _T("error ")  << err << std::endl;
                  }
                  else
                  {

                      windows.emplace(handle, window);

                      monitors.emplace(monitor, handle);
                      ShowWindow(handle, nCmdShow);
                      if(verbose)
                      {
                        Tcout << _T("Created window for monitor: ") << monitorInfo.szDevice << _T(" (") << pMonitorRect->left << _T(", ") << pMonitorRect->top << _T(")-(") << pMonitorRect->right << _T(", ") << pMonitorRect->bottom << _T(")") << std::endl;
                      }
                  }

                }
              }
                return TRUE;
             });
            break;
    }

    case WM_ERASEBKGND:
      return 0;

    case WM_COMMAND:
      {
        switch(LOWORD(wParam))
        {
          case unlockControlId:
            {
              switch(HIWORD(wParam))
              {
                case BN_CLICKED:
                  {
                    tstring password;
                    password.resize((std::size_t)GetWindowTextLength(passwordControl) + 1);
                    GetWindowText(passwordControl, (LPTSTR)password.data(), (int)password.size());
                    SetWindowText(passwordControl, _T(""));
                    std::size_t nullPosition = password.find_first_of(_T('\0'));

                    if(nullPosition != tstring::npos)
                    {
                      password.resize(nullPosition);
                    }

                    if(hashPassword(password) == hashedPassword)
                    {
                      for(auto i : windows)
                      {
                        DestroyWindow(std::get<1>(i).window);
                      }

                      return 0;
                    }

                    SendMessage(backgroundImageControl, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)backgroundImageError);

                    tostringstream str;

                    if(debugPassword)
                    {
                      str << _T("Password = \"") << password << _T("\". Type \"exit\" to unlock.");
                      MessageBox(window.window, str.str().c_str(), _T(""), MB_OK);
                    }

                    SetFocus(passwordControl);
                    break;
                  }
              }

              break;
            }

          case startKeyboardControlId:
            {
              switch(HIWORD(wParam))
              {
                case BN_CLICKED:
                  {
                    EnableTouchKeyboard(window, true);
                    SetFocus(passwordControl);
                    break;
                  }
              }

              break;
            }
        }

        break;
      }

    case WM_DRAWITEM:
      {
        LPDRAWITEMSTRUCT drawItemStruct = (LPDRAWITEMSTRUCT)lParam;
        HDC dc = drawItemStruct->hDC;

        switch((UINT)wParam)
        {
          case startKeyboardControlId:
            {
              HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
              DrawIconEx(dc,
                  drawItemStruct->rcItem.left,
                  drawItemStruct->rcItem.top,
                  keyboardIcon,
                  drawItemStruct->rcItem.right - drawItemStruct->rcItem.left,
                  drawItemStruct->rcItem.bottom - drawItemStruct->rcItem.top,
                  0,
                  brush,
                  DI_NORMAL);
              DeleteObject(brush);
              return 0;
            }
        }

        break;
      }

    case WM_CTLCOLORSTATIC:
      {
        HDC hdcStatic = (HDC) wParam;
        SetTextColor(hdcStatic, RGB(255,255,255));
        SetBkColor(hdcStatic, RGB(0,0,0));
        return (INT_PTR)CreateSolidBrush(RGB(0,0,0));
      }

    case WM_PAINT:
      {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(window.window, &ps);
        HBRUSH backgroundBrush = CreateSolidBrush(RGB(0, 0, 0));
        //HBRUSH oldBrush = SelectObject(dc, backgroundBrush);
        FillRect(dc, &ps.rcPaint, backgroundBrush);
        //SelectObject(dc, oldBrush);
        DeleteObject(backgroundBrush);
        EndPaint(window.window, &ps);
        break;
      }

    case DM_GETDEFID:
      {
        return MAKELRESULT(unlockControlId, DC_HASDEFID);
      }

    case WM_ACTIVATE:
      {
        switch(wParam)
        {
          case WA_INACTIVE:
            SetWindowPos(window.window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            break;
        }

        break;
      }

    default: /* for messages that we don't deal with */
      return DefWindowProc(window.window, messageId, wParam, lParam);
  }

  return 0;
}

LRESULT CALLBACK Application::StaticKeyboardHookProcedure(int nCode, WPARAM wParam, LPARAM lParam)
{
  if(nCode < 0)
  {
    return CallNextHookEx((HHOOK)NULL, nCode, wParam, lParam);
  }

  static std::unordered_set<DWORD> *pCurrentlyPushedKeys = nullptr;

  if(!pCurrentlyPushedKeys)
    pCurrentlyPushedKeys = new std::unordered_set<DWORD>();

  LPKBDLLHOOKSTRUCT hookStruct = (LPKBDLLHOOKSTRUCT)lParam;

  bool blockKey = false;

  if(hookStruct->flags & LLKHF_ALTDOWN)
  {
    blockKey = true;
  }

  switch(hookStruct->vkCode)
  {
    case VK_ESCAPE:
    case VK_LMENU:
    case VK_RMENU:
    case 0x48:
    case VK_LWIN:
    case VK_RWIN:
    case VK_LAUNCH_MAIL:
    case VK_LAUNCH_MEDIA_SELECT:
    case VK_LAUNCH_APP1:
    case VK_LAUNCH_APP2:
      blockKey = true;
      break;
  }

  if(hookStruct->flags & LLKHF_UP)
  {
    if(pCurrentlyPushedKeys->count(hookStruct->vkCode))
      blockKey = false;
    pCurrentlyPushedKeys->erase(hookStruct->vkCode);
  }
  else
  {
    if(!blockKey)
      pCurrentlyPushedKeys->insert(hookStruct->vkCode);
  }

  if(blockKey)
  {
    return 1;
  }

  return CallNextHookEx((HHOOK)NULL, nCode, wParam, lParam);
}

HRESULT Application::EnableWindowSystemTouchGestures(HWND window, bool enabled)
{
  PROPERTYKEY key;
  HRESULT retval = PSPropertyKeyFromString(L"{32CE38B2-2C9A-41B1-9BC5-B3784394AA44} 2", &key);

  if(retval != S_OK)
  {
    return retval;
  }

  IPropertyStore *propertyStore;
  retval = SHGetPropertyStoreForWindow(window, IID_PPV_ARGS(&propertyStore));

  if(!SUCCEEDED(retval))
  {
    return retval;
  }

  PROPVARIANT variable;
  variable.vt = VT_BOOL;
  variable.boolVal = enabled ? VARIANT_FALSE : VARIANT_TRUE;
  retval = propertyStore->SetValue(key, variable);
  propertyStore->Release();
  return retval;
}

void Application::EnableTouchKeyboard(Window &parentWindow, bool enabled)
{
  if(!enabled)
  {
    HWND touchKeyboardWindow = NULL;
    tstring className;
    auto checkWindowFn = [&](HWND window)->BOOL
    {
      className.reserve(128);
      className.resize(64);
      int classNameSize;

      do
      {
        classNameSize = GetClassName(window, &className[0], className.size());

        if(classNameSize == 0)
        {
          break;
        }

        className.resize(className.size() * 2);
      }
      while((std::size_t)classNameSize + 1 >= className.size());

      if(classNameSize == 0)
      {
        return TRUE;
      }

      className.resize(classNameSize);

      if(className == _T("IPTip_Main_Window"))
      {
        DWORD pid;
        GetWindowThreadProcessId(window, &pid);
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

        if(process == NULL)
        {
          return TRUE;
        }

        tstring processImageFileName;
        processImageFileName.reserve(128);
        processImageFileName.resize(64);
        DWORD processImageFileNameSize = processImageFileName.size();

        do
        {
          if(!QueryFullProcessImageName(process, 0, &processImageFileName[0], &processImageFileNameSize))
          {
            CloseHandle(process);
            process = NULL;
            break;
          }

          processImageFileName.resize(processImageFileName.size() * 2);
        }
        while(processImageFileNameSize + 1 >= processImageFileName.size());

        if(process == NULL)
        {
          return TRUE;
        }

        CloseHandle(process);

        processImageFileName.resize(processImageFileNameSize);
        std::size_t lastSlash = processImageFileName.find_last_of(_T("\\/"));

        if(lastSlash != tstring::npos)
        {
          processImageFileName.erase(0, lastSlash + 1);
        }

        for(TCHAR &ch : processImageFileName)
        {
          ch = _totlower(ch);
        }

        if(processImageFileName != _T("tabtip.exe") && processImageFileName != _T("tabtip32.exe"))
        {
          return TRUE;
        }

        touchKeyboardWindow = window;
        return FALSE;
      }

      return TRUE;
    };
    HWND foundWindow = FindWindow(_T("IPTip_Main_Window"), NULL);

    if(foundWindow != NULL)
    {
      if(checkWindowFn(foundWindow))
      {
        EnumWindows(checkWindowFn);
      }
    }
    else
    {
      EnumWindows(checkWindowFn);
    }

    if(touchKeyboardWindow != NULL)
    {
      PostMessage(touchKeyboardWindow, WM_SYSCOMMAND, (WPARAM)SC_CLOSE, (LPARAM)MAKELPARAM(0, -1));
    }

    return;
  }

  STARTUPINFO startupInfo;
  ZeroMemory(&startupInfo, sizeof(startupInfo)); // finish
  PROCESS_INFORMATION processInformation;
  ZeroMemory(&processInformation, sizeof(processInformation)); // finish
  startupInfo.cb = sizeof(startupInfo);
  tstring programFiles;
  programFiles.resize(1);
  LPCTSTR programFilesEnvironmentVariableName = _T("PROGRAMFILES");

  if(IsWow64Process())
  {
    programFilesEnvironmentVariableName = _T("ProgramW6432");
  }

  programFiles.resize(GetEnvironmentVariable(programFilesEnvironmentVariableName, &programFiles[0], programFiles.size()) + 1);
  GetEnvironmentVariable(programFilesEnvironmentVariableName, &programFiles[0], programFiles.size());
  std::size_t nullPosition = programFiles.find_first_of(_T('\0'));

  if(nullPosition != tstring::npos)
  {
    programFiles.resize(nullPosition);
  }

  tstring command = programFiles + _T("\\Common Files\\microsoft shared\\ink\\TabTip.exe");
  TCHAR *commandBuffer = new TCHAR[command.size() + 3];
  commandBuffer[0] = _T('\"');

  for(std::size_t i = 0; i < command.size(); i++)
  {
    commandBuffer[i + 1] = command[i];
  }

  commandBuffer[command.size() + 1] = _T('\"');
  commandBuffer[command.size() + 2] = _T('\0');

  DWORD error = ERROR_SUCCESS;
  tstring functionName = _T("CreateProcess");

  if(CreateProcess(NULL,
        commandBuffer,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &startupInfo,
        &processInformation))
  {
    CloseHandle(processInformation.hProcess);
    CloseHandle(processInformation.hThread);
  }
  else
  {
    error = GetLastError();

    if(error == ERROR_ELEVATION_REQUIRED)
    {
      error = ERROR_SUCCESS;
      functionName = _T("ShellExecute");

      if(reinterpret_cast<std::intptr_t>(ShellExecute(parentWindow.window, _T("open"), command.c_str(), _T(""), NULL, SW_SHOWDEFAULT)) <= 32)
      {
        error = GetLastError();
      }
    }
  }

  delete []commandBuffer;
}

bool Application::IsWow64Process()
{
  BOOL retval;
  LPFN_ISWOW64PROCESS fn = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(_T("kernel32")), "IsWow64Process");

  if(!fn)
  {
    return false;
  }

  if(!fn(GetCurrentProcess(), &retval))
  {
    return false;
  }

  return retval ? true : false;
}