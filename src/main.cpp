#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <mmsystem.h>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winmm.lib")

enum EventType : uint8_t {
    EV_MOVE = 1, EV_MOUSE_DOWN, EV_MOUSE_UP, EV_WHEEL, EV_KEY_DOWN, EV_KEY_UP
};

#pragma pack(push, 1)
struct MacroEvent {
    uint32_t delayUs;
    uint8_t type;
    uint8_t flags;
    uint16_t data;
    int32_t x;
    int32_t y;
};

struct ProfileHeader {
    char magic[4];
    uint16_t version;
    uint16_t reserved;
    uint32_t countA;
    uint32_t countB;
};

struct AppHotkey {
    uint32_t modifiers;
    uint32_t virtualKey;
};

struct SettingsFile {
    char magic[4];
    uint16_t version;
    uint8_t dark;
    uint8_t language;
    uint8_t mode;
    uint8_t activeTrack;
    uint8_t speedIndex;
    uint8_t infiniteLoop;
    uint8_t reserved;
    uint8_t reserved2;
    uint16_t repeats;
    AppHotkey hotkeys[3];
};
#pragma pack(pop)

struct Stats {
    uint32_t events = 0;
    uint32_t moves = 0;
    uint32_t clicks = 0;
    uint32_t keys = 0;
    uint64_t durationUs = 0;
};

enum ControlId {
    C_NONE, C_THEME, C_LANGUAGE, C_TAB_A, C_TAB_B, C_RECORD, C_PLAY, C_STOP,
    C_PAUSE, C_MODE, C_REPEAT_MINUS, C_REPEAT_PLUS, C_INFINITE, C_SPEED,
    C_SPEED_MINUS, C_SPEED_PLUS, C_OPEN, C_SAVE, C_CLEAR, C_SETTINGS,
    C_SETTINGS_BACK, C_HOTKEY_RECORD, C_HOTKEY_PLAY, C_HOTKEY_STOP, C_RESET_HOTKEYS,
    C_CONTROL_COUNT
};

struct HitTarget { RECT rect; ControlId id; bool enabled; };

enum StatusKind { ST_READY, ST_RECORDING, ST_PLAYING, ST_STOPPED, ST_SAVED, ST_LOADED, ST_ERROR };

static HINSTANCE gInstance = nullptr;
static HWND gWindow = nullptr;
static HHOOK gMouseHook = nullptr;
static HHOOK gKeyboardHook = nullptr;
static HANDLE gStopEvent = nullptr;
static HANDLE gPlaybackThread = nullptr;
static CRITICAL_SECTION gSequenceLock;
static std::vector<MacroEvent> gTracks[2];
static Stats gTrackStats[2];
static std::vector<HitTarget> gHits;
static volatile LONG gRecording = FALSE;
static volatile LONG gPlaying = FALSE;
static volatile LONG gPaused = FALSE;
static volatile LONG gPlaybackCycle = 0;
static int gActiveTrack = 0;
static int gMode = 0; // 0: selected sequence, 1: A then B
static int gRepeats = 1;
static int gSpeedIndex = 3;
static AppHotkey gHotkeys[3] = {{0, VK_F8}, {0, VK_F9}, {0, VK_F10}};
static bool gInfinite = false;
static bool gDark = true;
static int gLanguage = 0; // 0 English, 1 Spanish
static bool gShowSettings = false;
static int gCaptureHotkey = -1;
static bool gRestoreCompactRect = false;
static RECT gCompactRect = {};
static bool gAutoCompact = false;
static bool gHasExpandedRect = false;
static RECT gExpandedRect = {};
static UINT gDpi = 96;
static ControlId gHover = C_NONE;
static StatusKind gStatus = ST_READY;
static uint64_t gLastEventUs = 0;
static uint64_t gLastMoveUs = 0;
static POINT gLastMovePoint = {INT32_MIN, INT32_MIN};
static LARGE_INTEGER gCounterFrequency = {};
static LARGE_INTEGER gRecordOrigin = {};
static std::wstring gCurrentProfile = L"Untitled";

static constexpr UINT WM_APP_PLAYBACK_DONE = WM_APP + 1;
static constexpr UINT HOTKEY_RECORD = 101;
static constexpr UINT HOTKEY_PLAY = 102;
static constexpr UINT HOTKEY_STOP = 103;

enum TextId {
    TX_SEQUENCE, TX_SEQUENCE_A, TX_SEQUENCE_B, TX_SINGLE, TX_ALTERNATE,
    TX_RECORD, TX_FINISH, TX_PLAY, TX_STOP, TX_READY, TX_RECORDING, TX_PLAYING,
    TX_STOPPED, TX_SAVED, TX_LOADED, TX_ERROR, TX_LOOP_SETTINGS, TX_MODE, TX_REPETITIONS,
    TX_INFINITE, TX_SPEED, TX_PROFILE, TX_OPEN, TX_SAVE, TX_CLEAR,
    TX_EMPTY, TX_RECORDED, TX_CYCLES, TX_CONFIRM_CLEAR, TX_CONFIRM_TITLE,
    TX_BOTH_REQUIRED, TX_NO_ACTIONS, TX_COUNT
};

static const wchar_t* EN[TX_COUNT] = {
    L"SEQUENCE", L"Recording A", L"Recording B", L"Single", L"A → B loop",
    L"Record", L"Finish", L"Play", L"Stop", L"Ready", L"Recording", L"Playing",
    L"Stopped", L"Profile saved", L"Profile loaded", L"Something went wrong", L"PLAYBACK", L"Mode", L"Repetitions",
    L"Infinite loop", L"Speed", L"PROFILE", L"Open", L"Save", L"Clear",
    L"Empty", L"recorded", L"cycles", L"Clear the selected recording?", L"Clear recording",
    L"Record both A and B before using alternating mode.", L"The selected recording is empty."
};

static const wchar_t* ES[TX_COUNT] = {
    L"GRABACIÓN", L"Grabación A", L"Grabación B", L"Normal", L"Bucle A → B",
    L"Grabar", L"Finalizar", L"Reproducir", L"Detener", L"Listo", L"Grabando", L"Reproduciendo",
    L"Detenido", L"Perfil guardado", L"Perfil cargado", L"Ocurrió un error", L"REPRODUCCIÓN", L"Modo", L"Repeticiones",
    L"Bucle infinito", L"Velocidad", L"PERFIL", L"Abrir", L"Guardar", L"Limpiar",
    L"Vacía", L"grabados", L"ciclos", L"¿Limpiar la grabación seleccionada?", L"Limpiar grabación",
    L"Graba A y B antes de usar el modo alternado.", L"La grabación seleccionada está vacía."
};

static const wchar_t* T(TextId id) { return gLanguage == 0 ? EN[id] : ES[id]; }
static int S(int value) { return MulDiv(value, (int)gDpi, 96); }
static COLORREF Hex(BYTE r, BYTE g, BYTE b) { return RGB(r, g, b); }

struct Palette {
    COLORREF background, background2, surface, surface2, border, text, muted, accent, accent2, accentHover;
    COLORREF red, green, shadow, input;
};

static Palette Colors() {
    if (gDark) return {
        Hex(10,9,24), Hex(28,16,45), Hex(27,24,45), Hex(39,34,62), Hex(72,61,100), Hex(255,248,255),
        Hex(190,176,207), Hex(154,119,255), Hex(255,126,195), Hex(184,151,255), Hex(255,103,139),
        Hex(103,224,193), Hex(6,5,15), Hex(48,41,73)
    };
    return {
        Hex(255,247,252), Hex(239,234,255), Hex(255,255,255), Hex(250,246,255), Hex(222,207,239), Hex(48,36,60),
        Hex(119,102,137), Hex(137,98,245), Hex(245,111,177), Hex(157,119,255), Hex(232,82,121),
        Hex(35,174,139), Hex(211,194,224), Hex(241,233,249)
    };
}

static uint64_t CounterUs() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart - gRecordOrigin.QuadPart) * 1000000LL / gCounterFrequency.QuadPart);
}

static std::wstring AppDataDirectory() {
    wchar_t buffer[MAX_PATH] = {};
    DWORD length = GetEnvironmentVariableW(L"APPDATA", buffer, MAX_PATH);
    std::wstring directory = length ? std::wstring(buffer, length) : L".";
    directory += L"\\TaskFlow";
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory;
}

static std::wstring SettingsPath() { return AppDataDirectory() + L"\\settings.bin"; }
static std::wstring AutosavePath() { return AppDataDirectory() + L"\\last-session.taskflow"; }

static bool WriteAll(HANDLE file, const void* data, DWORD size) {
    DWORD written = 0;
    return WriteFile(file, data, size, &written, nullptr) && written == size;
}

static bool ReadAll(HANDLE file, void* data, DWORD size) {
    DWORD read = 0;
    return ReadFile(file, data, size, &read, nullptr) && read == size;
}

static bool SaveProfile(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    EnterCriticalSection(&gSequenceLock);
    ProfileHeader header = {{'T','F','P','2'}, 2, 0, (uint32_t)gTracks[0].size(), (uint32_t)gTracks[1].size()};
    bool ok = WriteAll(file, &header, sizeof(header));
    if (ok && header.countA) ok = WriteAll(file, gTracks[0].data(), header.countA * sizeof(MacroEvent));
    if (ok && header.countB) ok = WriteAll(file, gTracks[1].data(), header.countB * sizeof(MacroEvent));
    LeaveCriticalSection(&gSequenceLock);
    CloseHandle(file);
    return ok;
}

static bool LoadProfile(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    ProfileHeader header = {};
    bool ok = ReadAll(file, &header, sizeof(header));
    ok = ok && memcmp(header.magic, "TFP2", 4) == 0 && header.version == 2;
    ok = ok && header.countA <= 2000000 && header.countB <= 2000000;
    std::vector<MacroEvent> a, b;
    if (ok) {
        a.resize(header.countA); b.resize(header.countB);
        if (header.countA) ok = ReadAll(file, a.data(), header.countA * sizeof(MacroEvent));
        if (ok && header.countB) ok = ReadAll(file, b.data(), header.countB * sizeof(MacroEvent));
    }
    CloseHandle(file);
    if (!ok) return false;
    EnterCriticalSection(&gSequenceLock);
    gTracks[0].swap(a); gTracks[1].swap(b);
    for (int track = 0; track < 2; ++track) {
        gTrackStats[track] = {};
        gTrackStats[track].events = (uint32_t)gTracks[track].size();
        for (const auto& event : gTracks[track]) {
            gTrackStats[track].durationUs += event.delayUs;
            if (event.type == EV_MOVE) gTrackStats[track].moves++;
            else if (event.type == EV_MOUSE_DOWN) gTrackStats[track].clicks++;
            else if (event.type == EV_KEY_DOWN) gTrackStats[track].keys++;
        }
    }
    LeaveCriticalSection(&gSequenceLock);
    return true;
}

static void SaveSettings() {
    SettingsFile settings = {};
    memcpy(settings.magic, "TFS3", 4); settings.version = 3;
    settings.dark = (uint8_t)gDark; settings.language = (uint8_t)gLanguage;
    settings.mode = (uint8_t)gMode; settings.activeTrack = (uint8_t)gActiveTrack;
    settings.speedIndex = (uint8_t)gSpeedIndex; settings.infiniteLoop = (uint8_t)gInfinite;
    settings.repeats = (uint16_t)gRepeats;
    for (int i = 0; i < 3; ++i) settings.hotkeys[i] = gHotkeys[i];
    HANDLE file = CreateFileW(SettingsPath().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, nullptr);
    if (file != INVALID_HANDLE_VALUE) { WriteAll(file, &settings, sizeof(settings)); CloseHandle(file); }
}

static void LoadSettings() {
    SettingsFile settings = {};
    HANDLE file = CreateFileW(SettingsPath().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    bool ok = ReadAll(file, &settings, sizeof(settings)); CloseHandle(file);
    if (!ok || memcmp(settings.magic, "TFS3", 4) != 0 || settings.version != 3) return;
    gDark = settings.dark != 0; gLanguage = settings.language > 1 ? 0 : settings.language;
    gMode = settings.mode > 1 ? 0 : settings.mode; gActiveTrack = settings.activeTrack > 1 ? 0 : settings.activeTrack;
    gSpeedIndex = settings.speedIndex > 9 ? 3 : settings.speedIndex; gInfinite = settings.infiniteLoop != 0;
    for (int i = 0; i < 3; ++i) {
        if (settings.hotkeys[i].virtualKey > 0 && settings.hotkeys[i].virtualKey < 256) gHotkeys[i] = settings.hotkeys[i];
    }
    gRepeats = std::clamp((int)settings.repeats, 1, 999);
}

static Stats TrackStats(int track) {
    EnterCriticalSection(&gSequenceLock);
    Stats stats = gTrackStats[track];
    LeaveCriticalSection(&gSequenceLock);
    return stats;
}

static void AddEvent(uint8_t type, uint16_t data, int x, int y, uint8_t flags = 0) {
    uint64_t now = CounterUs();
    uint64_t delay = now >= gLastEventUs ? now - gLastEventUs : 0;
    MacroEvent event = {(uint32_t)std::min<uint64_t>(delay, 0xFFFFFFFFULL), type, flags, data, x, y};
    EnterCriticalSection(&gSequenceLock);
    gTracks[gActiveTrack].push_back(event);
    Stats& stats = gTrackStats[gActiveTrack];
    stats.events++; stats.durationUs += event.delayUs;
    if (type == EV_MOVE) stats.moves++;
    else if (type == EV_MOUSE_DOWN) stats.clicks++;
    else if (type == EV_KEY_DOWN) stats.keys++;
    LeaveCriticalSection(&gSequenceLock);
    gLastEventUs = now;
}

static bool IsConfiguredHotkey(DWORD vk) {
    uint32_t current = 0;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) current |= MOD_CONTROL;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) current |= MOD_SHIFT;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) current |= MOD_ALT;
    if ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) current |= MOD_WIN;
    for (const auto& hotkey : gHotkeys) {
        if (hotkey.virtualKey == vk && (current & hotkey.modifiers) == hotkey.modifiers) return true;
    }
    return false;
}

static LRESULT CALLBACK MouseHookProc(int code, WPARAM message, LPARAM param) {
    if (code >= 0 && InterlockedCompareExchange(&gRecording, TRUE, TRUE) && !InterlockedCompareExchange(&gPaused, FALSE, FALSE)) {
        auto* data = reinterpret_cast<MSLLHOOKSTRUCT*>(param);
        if (!(data->flags & LLMHF_INJECTED)) {
            if (message == WM_MOUSEMOVE) {
                uint64_t now = CounterUs();
                int distance = std::abs(data->pt.x - gLastMovePoint.x) + std::abs(data->pt.y - gLastMovePoint.y);
                if (now - gLastMoveUs >= 12000 && (gLastMovePoint.x == INT32_MIN || distance >= 2)) {
                    AddEvent(EV_MOVE, 0, data->pt.x, data->pt.y);
                    gLastMoveUs = now; gLastMovePoint = data->pt;
                }
            } else if (message == WM_LBUTTONDOWN) AddEvent(EV_MOUSE_DOWN, 1, data->pt.x, data->pt.y);
            else if (message == WM_LBUTTONUP) AddEvent(EV_MOUSE_UP, 1, data->pt.x, data->pt.y);
            else if (message == WM_RBUTTONDOWN) AddEvent(EV_MOUSE_DOWN, 2, data->pt.x, data->pt.y);
            else if (message == WM_RBUTTONUP) AddEvent(EV_MOUSE_UP, 2, data->pt.x, data->pt.y);
            else if (message == WM_MBUTTONDOWN) AddEvent(EV_MOUSE_DOWN, 3, data->pt.x, data->pt.y);
            else if (message == WM_MBUTTONUP) AddEvent(EV_MOUSE_UP, 3, data->pt.x, data->pt.y);
            else if (message == WM_XBUTTONDOWN) AddEvent(EV_MOUSE_DOWN, HIWORD(data->mouseData) == XBUTTON1 ? 4 : 5, data->pt.x, data->pt.y);
            else if (message == WM_XBUTTONUP) AddEvent(EV_MOUSE_UP, HIWORD(data->mouseData) == XBUTTON1 ? 4 : 5, data->pt.x, data->pt.y);
            else if (message == WM_MOUSEWHEEL) AddEvent(EV_WHEEL, (uint16_t)(int16_t)HIWORD(data->mouseData), data->pt.x, data->pt.y, 0);
            else if (message == WM_MOUSEHWHEEL) AddEvent(EV_WHEEL, (uint16_t)(int16_t)HIWORD(data->mouseData), data->pt.x, data->pt.y, 1);
        }
    }
    return CallNextHookEx(gMouseHook, code, message, param);
}

static LRESULT CALLBACK KeyboardHookProc(int code, WPARAM message, LPARAM param) {
    if (code >= 0 && InterlockedCompareExchange(&gRecording, TRUE, TRUE) && !InterlockedCompareExchange(&gPaused, FALSE, FALSE)) {
        auto* data = reinterpret_cast<KBDLLHOOKSTRUCT*>(param);
        // Ignore releases from the hotkey that started recording.
        if (CounterUs() >= 350000 && !(data->flags & LLKHF_INJECTED) && !IsConfiguredHotkey(data->vkCode)) {
            uint8_t flags = (data->flags & LLKHF_EXTENDED) ? 1 : 0;
            if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) AddEvent(EV_KEY_DOWN, (uint16_t)data->vkCode, 0, 0, flags);
            else if (message == WM_KEYUP || message == WM_SYSKEYUP) AddEvent(EV_KEY_UP, (uint16_t)data->vkCode, 0, 0, flags);
        }
    }
    return CallNextHookEx(gKeyboardHook, code, message, param);
}

static void StopRecording();

static bool StartRecording() {
    if (InterlockedCompareExchange(&gPlaying, FALSE, FALSE)) return false;
    EnterCriticalSection(&gSequenceLock);
    gTracks[gActiveTrack].clear();
    gTracks[gActiveTrack].reserve(16384);
    gTrackStats[gActiveTrack] = {};
    LeaveCriticalSection(&gSequenceLock);
    QueryPerformanceCounter(&gRecordOrigin);
    gLastEventUs = 0; gLastMoveUs = 0; gLastMovePoint = {INT32_MIN, INT32_MIN};
    gMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, gInstance, 0);
    gKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, gInstance, 0);
    if (!gMouseHook || !gKeyboardHook) {
        if (gMouseHook) UnhookWindowsHookEx(gMouseHook);
        if (gKeyboardHook) UnhookWindowsHookEx(gKeyboardHook);
        gMouseHook = gKeyboardHook = nullptr; gStatus = ST_ERROR; return false;
    }
    InterlockedExchange(&gPaused, FALSE); InterlockedExchange(&gRecording, TRUE); gStatus = ST_RECORDING; InvalidateRect(gWindow, nullptr, FALSE);
    return true;
}

static void StopRecording() {
    if (!InterlockedExchange(&gRecording, FALSE)) return;
    if (gMouseHook) UnhookWindowsHookEx(gMouseHook);
    if (gKeyboardHook) UnhookWindowsHookEx(gKeyboardHook);
    gMouseHook = gKeyboardHook = nullptr;
    InterlockedExchange(&gPaused, FALSE); SaveProfile(AutosavePath());
    gStatus = ST_STOPPED; InvalidateRect(gWindow, nullptr, FALSE);
}

static void MouseInput(DWORD flags, LONG data = 0, int x = 0, int y = 0) {
    INPUT input = {}; input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags; input.mi.mouseData = data; input.mi.dx = x; input.mi.dy = y;
    SendInput(1, &input, sizeof(input));
}

static void MoveMouseAbsolute(int x, int y) {
    int left = GetSystemMetrics(SM_XVIRTUALSCREEN), top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = std::max(1, GetSystemMetrics(SM_CXVIRTUALSCREEN) - 1);
    int height = std::max(1, GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1);
    int absoluteX = MulDiv(x - left, 65535, width), absoluteY = MulDiv(y - top, 65535, height);
    MouseInput(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK, 0, absoluteX, absoluteY);
}

static void SendMouseButton(int button, bool down) {
    DWORD flag = 0; LONG data = 0;
    if (button == 1) flag = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    else if (button == 2) flag = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    else if (button == 3) flag = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    else if (button == 4 || button == 5) { flag = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP; data = button == 4 ? XBUTTON1 : XBUTTON2; }
    if (flag) MouseInput(flag, data);
}

static void SendKey(uint16_t vk, bool down, bool extended) {
    INPUT input = {}; input.type = INPUT_KEYBOARD; input.ki.wVk = vk;
    input.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP) | (extended ? KEYEVENTF_EXTENDEDKEY : 0);
    SendInput(1, &input, sizeof(input));
}

static bool WaitMicroseconds(uint64_t microseconds) {
    while (microseconds > 0) {
        if (WaitForSingleObject(gStopEvent, 0) == WAIT_OBJECT_0) return false;
        if (InterlockedCompareExchange(&gPaused, FALSE, FALSE)) { Sleep(16); continue; }
        DWORD milliseconds = (DWORD)std::min<uint64_t>(microseconds / 1000, 10);
        if (milliseconds == 0) milliseconds = 1;
        Sleep(milliseconds);
        uint64_t used = (uint64_t)milliseconds * 1000;
        microseconds = used >= microseconds ? 0 : microseconds - used;
    }
    return WaitForSingleObject(gStopEvent, 0) != WAIT_OBJECT_0;
}

struct PlaybackJob {
    std::vector<MacroEvent> a, b;
    int activeTrack, mode, repeats, speedPercent;
    bool infinite;
};

static bool PlaySequence(const std::vector<MacroEvent>& sequence, int speedPercent, bool keys[256], int& buttons) {
    for (const auto& event : sequence) {
        uint64_t delay = (uint64_t)event.delayUs * 100 / speedPercent;
        if (!WaitMicroseconds(delay)) return false;
        if (event.type == EV_MOVE) MoveMouseAbsolute(event.x, event.y);
        else if (event.type == EV_MOUSE_DOWN) { MoveMouseAbsolute(event.x, event.y); SendMouseButton(event.data, true); buttons |= 1 << event.data; }
        else if (event.type == EV_MOUSE_UP) { MoveMouseAbsolute(event.x, event.y); SendMouseButton(event.data, false); buttons &= ~(1 << event.data); }
        else if (event.type == EV_WHEEL) {
            MoveMouseAbsolute(event.x, event.y);
            MouseInput(event.flags ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL, (LONG)(int16_t)event.data);
        } else if (event.type == EV_KEY_DOWN) { SendKey(event.data, true, event.flags != 0); if (event.data < 256) keys[event.data] = true; }
        else if (event.type == EV_KEY_UP) { SendKey(event.data, false, event.flags != 0); if (event.data < 256) keys[event.data] = false; }
    }
    return true;
}

static DWORD WINAPI PlaybackThreadProc(void* parameter) {
    auto* job = reinterpret_cast<PlaybackJob*>(parameter);
    bool keys[256] = {}; int buttons = 0;
    timeBeginPeriod(1);
    WaitMicroseconds(350000);
    int cycle = 0;
    while (WaitForSingleObject(gStopEvent, 0) != WAIT_OBJECT_0 && (job->infinite || cycle < job->repeats)) {
        ++cycle; InterlockedExchange(&gPlaybackCycle, cycle);
        bool ok = true;
        if (job->mode == 0) ok = PlaySequence(job->activeTrack == 0 ? job->a : job->b, job->speedPercent, keys, buttons);
        else {
            ok = PlaySequence(job->a, job->speedPercent, keys, buttons);
            if (ok) ok = PlaySequence(job->b, job->speedPercent, keys, buttons);
        }
        if (!ok) break;
    }
    for (int key = 0; key < 256; ++key) if (keys[key]) SendKey((uint16_t)key, false, false);
    for (int button = 1; button <= 5; ++button) if (buttons & (1 << button)) SendMouseButton(button, false);
    timeEndPeriod(1);
    delete job;
    PostMessageW(gWindow, WM_APP_PLAYBACK_DONE, 0, 0);
    return 0;
}

static int SpeedPercent() {
    static const int values[] = {25, 50, 75, 100, 125, 150, 200, 300, 400, 500};
    return values[std::clamp(gSpeedIndex, 0, 9)];
}

static std::wstring SpeedLabel() {
    switch (SpeedPercent()) {
        case 25: return L"0.25×"; case 50: return L"0.5×"; case 75: return L"0.75×";
        case 100: return L"1×"; case 125: return L"1.25×"; case 150: return L"1.5×";
        case 200: return L"2×"; case 300: return L"3×"; case 400: return L"4×"; default: return L"5×";
    }
}

static bool StartPlayback() {
    if (InterlockedCompareExchange(&gRecording, FALSE, FALSE) || InterlockedCompareExchange(&gPlaying, TRUE, TRUE)) return false;
    auto* job = new PlaybackJob();
    EnterCriticalSection(&gSequenceLock); job->a = gTracks[0]; job->b = gTracks[1]; LeaveCriticalSection(&gSequenceLock);
    if ((gMode == 0 && (gActiveTrack == 0 ? job->a.empty() : job->b.empty()))) {
        delete job; MessageBoxW(gWindow, T(TX_NO_ACTIONS), L"TaskFlow", MB_OK | MB_ICONINFORMATION); return false;
    }
    if (gMode == 1 && (job->a.empty() || job->b.empty())) {
        delete job; MessageBoxW(gWindow, T(TX_BOTH_REQUIRED), L"TaskFlow", MB_OK | MB_ICONINFORMATION); return false;
    }
    job->activeTrack = gActiveTrack; job->mode = gMode; job->repeats = gRepeats;
    job->speedPercent = SpeedPercent(); job->infinite = gInfinite;
    ResetEvent(gStopEvent); InterlockedExchange(&gPlaybackCycle, 0); InterlockedExchange(&gPaused, FALSE); InterlockedExchange(&gPlaying, TRUE);
    gStatus = ST_PLAYING;
    gPlaybackThread = CreateThread(nullptr, 0, PlaybackThreadProc, job, 0, nullptr);
    if (!gPlaybackThread) { delete job; InterlockedExchange(&gPlaying, FALSE); gStatus = ST_ERROR; return false; }
    InvalidateRect(gWindow, nullptr, FALSE); return true;
}

static void StopPlayback() {
    if (InterlockedCompareExchange(&gPlaying, TRUE, TRUE)) SetEvent(gStopEvent);
}

static void TogglePause() {
    bool active = InterlockedCompareExchange(&gRecording, TRUE, TRUE) || InterlockedCompareExchange(&gPlaying, TRUE, TRUE);
    if (!active) return;
    LONG paused = InterlockedCompareExchange(&gPaused, FALSE, FALSE);
    InterlockedExchange(&gPaused, !paused);
    if (paused && InterlockedCompareExchange(&gRecording, TRUE, TRUE)) gLastEventUs = CounterUs();
    InvalidateRect(gWindow, nullptr, FALSE);
}

static bool RegisterAppHotkeys() {
    UnregisterHotKey(gWindow, HOTKEY_RECORD); UnregisterHotKey(gWindow, HOTKEY_PLAY); UnregisterHotKey(gWindow, HOTKEY_STOP);
    bool record = RegisterHotKey(gWindow, HOTKEY_RECORD, gHotkeys[0].modifiers | MOD_NOREPEAT, gHotkeys[0].virtualKey) != FALSE;
    bool play = RegisterHotKey(gWindow, HOTKEY_PLAY, gHotkeys[1].modifiers | MOD_NOREPEAT, gHotkeys[1].virtualKey) != FALSE;
    bool stop = RegisterHotKey(gWindow, HOTKEY_STOP, gHotkeys[2].modifiers | MOD_NOREPEAT, gHotkeys[2].virtualKey) != FALSE;
    return record && play && stop;
}

static std::wstring VirtualKeyName(uint32_t key) {
    if (key >= VK_F1 && key <= VK_F24) return L"F" + std::to_wstring(key - VK_F1 + 1);
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) return std::wstring(1, (wchar_t)key);
    if (key == VK_SPACE) return L"Space"; if (key == VK_RETURN) return L"Enter";
    if (key == VK_TAB) return L"Tab"; if (key == VK_ESCAPE) return L"Esc";
    if (key == VK_DELETE) return L"Delete"; if (key == VK_INSERT) return L"Insert";
    UINT scan = MapVirtualKeyW(key, MAPVK_VK_TO_VSC); LONG parameter = (LONG)(scan << 16);
    wchar_t name[64] = {}; if (GetKeyNameTextW(parameter, name, 64)) return name;
    return L"Key " + std::to_wstring(key);
}

static std::wstring HotkeyText(const AppHotkey& hotkey) {
    std::wstring text;
    if (hotkey.modifiers & MOD_CONTROL) text += L"Ctrl+";
    if (hotkey.modifiers & MOD_SHIFT) text += L"Shift+";
    if (hotkey.modifiers & MOD_ALT) text += L"Alt+";
    if (hotkey.modifiers & MOD_WIN) text += L"Win+";
    return text + VirtualKeyName(hotkey.virtualKey);
}

static std::wstring HotkeySummary() {
    return HotkeyText(gHotkeys[0]) + L"  ·  " + HotkeyText(gHotkeys[1]) + L"  ·  " + HotkeyText(gHotkeys[2]);
}

static void ApplyTitlebarTheme() {
    BOOL dark = gDark ? TRUE : FALSE;
    DwmSetWindowAttribute(gWindow, 20, &dark, sizeof(dark));
}

static HFONT Font(int size, int weight = FW_NORMAL, const wchar_t* face = L"Segoe UI Variable Text") {
    return CreateFontW(-S(size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, face);
}

static void FillRectColor(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color); FillRect(dc, &rect, brush); DeleteObject(brush);
}

static COLORREF BlendColor(COLORREF from, COLORREF to, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);
    auto blend = [amount](BYTE a, BYTE b) { return (BYTE)(a + (b - a) * amount); };
    return RGB(blend(GetRValue(from), GetRValue(to)), blend(GetGValue(from), GetGValue(to)), blend(GetBValue(from), GetBValue(to)));
}

static void GradientRectFill(HDC dc, const RECT& rect, COLORREF first, COLORREF second, bool horizontal = false) {
    TRIVERTEX vertices[2] = {
        {(LONG)rect.left,(LONG)rect.top,(COLOR16)(GetRValue(first)<<8),(COLOR16)(GetGValue(first)<<8),(COLOR16)(GetBValue(first)<<8),0xFF00},
        {(LONG)rect.right,(LONG)rect.bottom,(COLOR16)(GetRValue(second)<<8),(COLOR16)(GetGValue(second)<<8),(COLOR16)(GetBValue(second)<<8),0xFF00}
    };
    GRADIENT_RECT gradient = {0,1};
    GradientFill(dc, vertices, 2, &gradient, 1, horizontal ? GRADIENT_FILL_RECT_H : GRADIENT_FILL_RECT_V);
}

static void RoundFill(HDC dc, const RECT& rect, int radius, COLORREF fill, COLORREF border, int borderWidth = 1) {
    HPEN pen = CreatePen(PS_SOLID, S(borderWidth), border); HBRUSH brush = CreateSolidBrush(fill);
    auto oldPen = SelectObject(dc, pen); auto oldBrush = SelectObject(dc, brush);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, S(radius), S(radius));
    SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(pen); DeleteObject(brush);
}

static void DrawString(HDC dc, const wchar_t* text, RECT rect, HFONT font, COLORREF color, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color); auto old = SelectObject(dc, font);
    DrawTextW(dc, text, -1, &rect, format); SelectObject(dc, old);
}

static void DrawDecorations(HDC dc, int width, int height, const Palette& c) {
    HBRUSH pink = CreateSolidBrush(BlendColor(c.background2, c.accent2, gDark ? 0.18f : 0.24f));
    HBRUSH violet = CreateSolidBrush(BlendColor(c.background, c.accent, gDark ? 0.15f : 0.18f));
    auto oldBrush = SelectObject(dc, pink); auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, width-S(180), -S(80), width+S(60), S(150));
    SelectObject(dc, violet); Ellipse(dc, -S(100), height-S(145), S(135), height+S(90));
    Ellipse(dc, width-S(74), height-S(120), width-S(20), height-S(66));
    SelectObject(dc, pink); Ellipse(dc, S(16), S(88), S(48), S(120));
    Ellipse(dc, width-S(235), S(20), width-S(203), S(52));
    SelectObject(dc, violet); Ellipse(dc, width/2-S(24), height-S(42), width/2+S(24), height+S(6));
    SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(pink); DeleteObject(violet);
    HFONT sparkle = Font(16, FW_SEMIBOLD, L"Segoe UI Symbol");
    DrawString(dc,L"✦",{width-S(105),S(74),width-S(75),S(102)},sparkle,BlendColor(c.accent2,c.background,0.25f),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    DrawString(dc,L"✧",{S(18),height-S(105),S(48),height-S(75)},sparkle,BlendColor(c.accent,c.background,0.30f),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    DeleteObject(sparkle);
}

static void AddHit(ControlId id, RECT rect, bool enabled = true) { gHits.push_back({rect, id, enabled}); }

static void DrawButton(HDC dc, ControlId id, RECT rect, const wchar_t* label, bool primary, bool enabled,
                       HFONT font, const Palette& c) {
    float hover = enabled && gHover == id ? 1.0f : 0.0f;
    COLORREF fill = primary ? BlendColor(c.accent, c.accentHover, hover) : BlendColor(c.surface2, c.input, hover);
    COLORREF border = primary ? fill : c.border; COLORREF text = enabled ? (primary ? RGB(255,255,255) : c.text) : c.muted;
    if (enabled && (primary || hover > 0.05f)) { RECT shadow = rect; OffsetRect(&shadow, 0, S(3)); RoundFill(dc, shadow, 10, BlendColor(c.shadow, c.background, 0.45f), BlendColor(c.shadow, c.background, 0.45f), 0); }
    RoundFill(dc, rect, 10, fill, border); DrawString(dc, label, rect, font, text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    AddHit(id, rect, enabled);
}

static void DrawToggle(HDC dc, RECT rect, bool on, const Palette& c) {
    int width = rect.right - rect.left, height = rect.bottom - rect.top;
    RoundFill(dc, rect, height / 2, on ? c.accent : c.border, on ? c.accent : c.border, 0);
    int diameter = height - S(6); int left = on ? rect.right - diameter - S(3) : rect.left + S(3);
    RECT knob = {left, rect.top + S(3), left + diameter, rect.top + S(3) + diameter};
    HBRUSH brush = CreateSolidBrush(RGB(255,255,255)); auto old = SelectObject(dc, brush);
    Ellipse(dc, knob.left, knob.top, knob.right, knob.bottom); SelectObject(dc, old); DeleteObject(brush);
    (void)width;
}

static void DrawFlag(HDC dc, RECT rect, bool spanish) {
    HFONT emoji = Font(17, FW_NORMAL, L"Segoe UI Emoji");
    DrawString(dc, spanish ? L"\xD83C\xDDEA\xD83C\xDDF8" : L"\xD83C\xDDFA\xD83C\xDDF8",
        rect, emoji, gDark ? RGB(255,255,255) : RGB(48,36,60), DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    DeleteObject(emoji);
}

static std::wstring Number(uint64_t value) { return std::to_wstring(value); }

static std::wstring CurrentStatusText() {
    bool recording = InterlockedCompareExchange(&gRecording, TRUE, TRUE) != FALSE;
    bool playing = InterlockedCompareExchange(&gPlaying, TRUE, TRUE) != FALSE;
    bool paused = InterlockedCompareExchange(&gPaused, TRUE, TRUE) != FALSE;
    if (paused) return gLanguage == 0 ? L"Paused" : L"Pausado";
    if (recording) return std::wstring(T(TX_RECORDING)) + L" " + (gActiveTrack == 0 ? L"A" : L"B");
    if (playing) return std::wstring(T(TX_PLAYING)) + L" · " + Number(InterlockedCompareExchange(&gPlaybackCycle,0,0));
    if (gStatus == ST_SAVED) return T(TX_SAVED); if (gStatus == ST_LOADED) return T(TX_LOADED);
    if (gStatus == ST_STOPPED) return T(TX_STOPPED); if (gStatus == ST_ERROR) return T(TX_ERROR);
    return T(TX_READY);
}

static void DrawCompactContent(HDC dc, int width, int height, HFONT titleFont, HFONT bodyFont, HFONT bodyBold, HFONT subtitleFont, const Palette& c) {
    RECT shadow = {S(17),S(17),width-S(13),height-S(11)}; RoundFill(dc, shadow, 18, c.shadow, c.shadow, 0);
    RECT glow = {S(14),S(12),width-S(16),height-S(16)}; RoundFill(dc, glow, 18, c.surface, BlendColor(c.accent2,c.border,0.52f));
    RECT logo = {S(30),S(25),S(66),S(61)}; RoundFill(dc,logo,11,c.accent,c.accent,0);
    DrawString(dc,L"▶",logo,bodyBold,RGB(255,255,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    DrawString(dc,L"TaskFlow",{S(78),S(19),S(255),S(49)},titleFont,c.text);
    std::wstring status=CurrentStatusText(); DrawString(dc,status.c_str(),{S(79),S(45),S(310),S(68)},subtitleFont,c.muted);
    std::wstring badge = std::wstring(L"A/B  ·  ") + (gMode == 0 ? T(TX_SINGLE) : T(TX_ALTERNATE));
    DrawString(dc,badge.c_str(),{width-S(250),S(24),width-S(30),S(58)},subtitleFont,c.muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);

    int left=S(26), right=width-S(28), gap=S(8), buttonWidth=(right-left-gap*3)/4;
    int top=std::max(S(82),height-S(88)), bottom=height-S(30);
    bool recording=gRecording!=FALSE,playing=gPlaying!=FALSE,active=recording||playing,paused=gPaused!=FALSE;
    RECT record={left,top,left+buttonWidth,bottom};
    RECT playPause={record.right+gap,top,record.right+gap+buttonWidth,bottom};
    RECT stop={playPause.right+gap,top,playPause.right+gap+buttonWidth,bottom};
    RECT settings={stop.right+gap,top,right,bottom};
    DrawButton(dc,C_RECORD,record,recording?T(TX_FINISH):T(TX_RECORD),true,!playing,bodyBold,c);
    const wchar_t* centerLabel=active?(paused?(gLanguage?L"Continuar":L"Resume"):(gLanguage?L"Pausar":L"Pause")):T(TX_PLAY);
    DrawButton(dc,active?C_PAUSE:C_PLAY,playPause,centerLabel,false,!recording||active,bodyBold,c);
    DrawButton(dc,C_STOP,stop,T(TX_STOP),false,active,bodyBold,c);
    DrawButton(dc,C_SETTINGS,settings,gLanguage?L"Ajustes":L"Settings",false,!active,bodyFont,c);
}

static void DrawSettingsContent(HDC dc, int width, int height, HFONT titleFont, HFONT bodyFont, HFONT bodyBold, HFONT subtitleFont, HFONT sectionFont, const Palette& c) {
    RECT back={S(28),S(24),S(108),S(60)};DrawButton(dc,C_SETTINGS_BACK,back,gLanguage?L"← Volver":L"← Back",false,true,bodyBold,c);
    DrawString(dc,gLanguage?L"Configuración":L"Settings",{S(132),S(20),width-S(220),S(55)},titleFont,c.text);
    DrawString(dc,gLanguage?L"Personaliza los controles, apariencia y reproducción.":L"Customize controls, appearance and playback.",{S(133),S(51),width-S(220),S(75)},subtitleFont,c.muted);
    RECT languageRect={width-S(180),S(26),width-S(105),S(60)};RoundFill(dc,languageRect,9,c.surface,c.border);DrawFlag(dc,{languageRect.left+S(8),languageRect.top+S(3),languageRect.left+S(34),languageRect.bottom-S(3)},gLanguage==1);DrawString(dc,gLanguage?L"ES":L"EN",{languageRect.left+S(39),languageRect.top,languageRect.right,languageRect.bottom},bodyBold,c.text);AddHit(C_LANGUAGE,languageRect);
    RECT themeRect={width-S(95),S(26),width-S(28),S(60)};DrawButton(dc,C_THEME,themeRect,gDark?L"LIGHT":L"DARK",false,true,sectionFont,c);

    RECT hotkeyCard={S(28),S(94),width-S(28),S(350)};RoundFill(dc,hotkeyCard,16,c.surface,c.border);
    DrawString(dc,gLanguage?L"ATAJOS GLOBALES":L"GLOBAL HOTKEYS",{S(52),S(107),width-S(52),S(132)},sectionFont,c.muted);
    DrawString(dc,gLanguage?L"Haz clic en un atajo y presiona la nueva combinación.":L"Click a hotkey, then press the new key combination.",{S(52),S(130),width-S(52),S(153)},subtitleFont,c.muted);
    const wchar_t* names[3]={gLanguage?L"Grabar / finalizar":L"Record / finish",gLanguage?L"Reproducir":L"Play",gLanguage?L"Detener":L"Stop"};
    ControlId ids[3]={C_HOTKEY_RECORD,C_HOTKEY_PLAY,C_HOTKEY_STOP};
    for(int i=0;i<3;++i){int y=S(166+i*54);DrawString(dc,names[i],{S(52),y,width/2-S(10),y+S(40)},bodyBold,c.text);RECT keyRect={width/2,y,width-S(52),y+S(40)};std::wstring key=gCaptureHotkey==i?(gLanguage?L"Presiona las teclas…":L"Press keys…"):HotkeyText(gHotkeys[i]);DrawButton(dc,ids[i],keyRect,key.c_str(),gCaptureHotkey==i,true,bodyBold,c);}
    RECT reset={width-S(218),S(312),width-S(52),S(338)};DrawButton(dc,C_RESET_HOTKEYS,reset,gLanguage?L"Restaurar atajos":L"Reset hotkeys",false,true,subtitleFont,c);

    RECT appearance={S(28),S(370),width/2-S(8),height-S(34)};RoundFill(dc,appearance,16,c.surface,c.border);
    DrawString(dc,gLanguage?L"APARIENCIA":L"APPEARANCE",{S(52),S(384),width/2-S(28),S(408)},sectionFont,c.muted);
    DrawString(dc,gLanguage?L"Tema oscuro":L"Dark theme",{S(52),S(425),S(220),S(457)},bodyBold,c.text);RECT darkToggle={width/2-S(92),S(428),width/2-S(44),S(454)};DrawToggle(dc,darkToggle,gDark,c);AddHit(C_THEME,darkToggle);
    DrawString(dc,gLanguage?L"Idioma":L"Language",{S(52),S(476),S(220),S(508)},bodyBold,c.text);RECT langButton={width/2-S(170),S(472),width/2-S(44),S(510)};RoundFill(dc,langButton,10,c.surface2,gHover==C_LANGUAGE?c.accent:c.border);DrawFlag(dc,{langButton.left+S(9),langButton.top+S(5),langButton.left+S(37),langButton.bottom-S(5)},gLanguage==1);DrawString(dc,gLanguage?L"Español":L"English",{langButton.left+S(42),langButton.top,langButton.right-S(8),langButton.bottom},bodyBold,c.text);AddHit(C_LANGUAGE,langButton);

    RECT playback={width/2+S(8),S(370),width-S(28),height-S(34)};RoundFill(dc,playback,16,c.surface,c.border);
    DrawString(dc,gLanguage?L"REPRODUCCIÓN":L"PLAYBACK",{width/2+S(32),S(384),width-S(50),S(408)},sectionFont,c.muted);
    DrawString(dc,gLanguage?L"Velocidad":L"Speed",{width/2+S(32),S(425),width-S(50),S(452)},bodyBold,c.text);
    RECT minus={width/2+S(32),S(464),width/2+S(76),S(506)},plus={width-S(94),S(464),width-S(50),S(506)};DrawButton(dc,C_SPEED_MINUS,minus,L"−",false,true,titleFont,c);DrawButton(dc,C_SPEED_PLUS,plus,L"+",false,true,titleFont,c);
    std::wstring speed=SpeedLabel();DrawString(dc,speed.c_str(),{width/2+S(82),S(464),width-S(100),S(506)},titleFont,c.text,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    DrawString(dc,gLanguage?L"Rango disponible: 0.25× — 5×":L"Available range: 0.25× — 5×",{width/2+S(32),S(518),width-S(50),S(546)},subtitleFont,c.muted);
}

static void PaintWindow(HDC target) {
    RECT client; GetClientRect(gWindow, &client); int width = client.right, height = client.bottom;
    HDC dc = CreateCompatibleDC(target); HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    auto oldBitmap = SelectObject(dc, bitmap); Palette c = Colors(); GradientRectFill(dc, client, c.background, c.background2, true); DrawDecorations(dc,width,height,c); gHits.clear();

    HFONT titleFont = Font(23, FW_BOLD, L"Segoe UI Variable Display"), subtitleFont = Font(10), sectionFont = Font(9, FW_BOLD);
    HFONT bodyFont = Font(11, FW_NORMAL), bodyBold = Font(11, FW_SEMIBOLD), bigFont = Font(22, FW_BOLD), metricFont = Font(18, FW_BOLD);

    bool compact = width < S(760) || height < S(520);
    if (gShowSettings) {
        DrawSettingsContent(dc,width,height,titleFont,bodyFont,bodyBold,subtitleFont,sectionFont,c);
    } else if (compact) {
        DrawCompactContent(dc,width,height,titleFont,bodyFont,bodyBold,subtitleFont,c);
    } else {
    RECT logo = {S(28), S(25), S(70), S(67)}; RoundFill(dc, logo, 13, c.accent, c.accent2, 1);
    HPEN whitePen = CreatePen(PS_SOLID, S(3), RGB(255,255,255)); auto oldPen = SelectObject(dc, whitePen); auto oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, S(39), S(34), S(58), S(53)); MoveToEx(dc, S(55), S(52), nullptr); LineTo(dc, S(63), S(58));
    SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(whitePen);
    DrawString(dc, L"TaskFlow", {S(82),S(21),S(300),S(51)}, titleFont, c.text);

    RECT languageRect = {width-S(252),S(27),width-S(176),S(61)};
    RoundFill(dc, languageRect, 9, c.surface, gHover==C_LANGUAGE?c.accent:c.border);
    DrawFlag(dc, {languageRect.left+S(8),languageRect.top+S(3),languageRect.left+S(34),languageRect.bottom-S(3)}, gLanguage==1);
    DrawString(dc, gLanguage==0?L"EN":L"ES", {languageRect.left+S(39),languageRect.top,languageRect.right,languageRect.bottom}, bodyBold, c.text);
    AddHit(C_LANGUAGE, languageRect);
    RECT themeRect = {width-S(166),S(27),width-S(102),S(61)};
    RoundFill(dc, themeRect, 9, c.surface, gHover==C_THEME?c.accent:c.border);
    DrawString(dc, gDark?L"LIGHT":L"DARK", themeRect, sectionFont, c.text, DT_CENTER|DT_VCENTER|DT_SINGLELINE); AddHit(C_THEME, themeRect);
    RECT settingsTop = {width-S(92),S(27),width-S(28),S(61)};
    DrawButton(dc,C_SETTINGS,settingsTop,L"⚙",false,!gRecording&&!gPlaying,bigFont,c);

    RECT mainCard = {S(28),S(92),width-S(28),S(280)}; RoundFill(dc, mainCard, 16, c.surface, c.border);
    DrawString(dc, T(TX_SEQUENCE), {S(50),S(107),S(190),S(128)}, sectionFont, c.muted);
    RECT tabA = {S(50),S(136),S(190),S(174)}, tabB = {S(198),S(136),S(338),S(174)};
    DrawButton(dc,C_TAB_A,tabA,T(TX_SEQUENCE_A),gActiveTrack==0,!gRecording&&!gPlaying,bodyBold,c);
    DrawButton(dc,C_TAB_B,tabB,T(TX_SEQUENCE_B),gActiveTrack==1,!gRecording&&!gPlaying,bodyBold,c);
    int underlineLeft=S(54+gActiveTrack*148);RECT underline={underlineLeft,S(178),underlineLeft+S(132),S(181)};GradientRectFill(dc,underline,c.accent,c.accent2,true);
    Stats activeStats=TrackStats(gActiveTrack);std::wstring trackInfo = activeStats.events?T(TX_RECORDED):T(TX_EMPTY);
    DrawString(dc, trackInfo.c_str(), {S(51),S(184),S(337),S(208)}, subtitleFont, c.muted);

    int controlsLeft = std::max(S(365), width-S(590));
    RECT recordRect = {controlsLeft,S(125),controlsLeft+S(120),S(202)};
    RECT playRect = {controlsLeft+S(128),S(125),controlsLeft+S(248),S(202)};
    RECT pauseRect = {controlsLeft+S(256),S(125),controlsLeft+S(376),S(202)};
    RECT stopRect = {controlsLeft+S(384),S(125),controlsLeft+S(504),S(202)};
    bool recording = InterlockedCompareExchange(&gRecording,TRUE,TRUE), playing = InterlockedCompareExchange(&gPlaying,TRUE,TRUE);
    bool paused = InterlockedCompareExchange(&gPaused,TRUE,TRUE);
    DrawButton(dc,C_RECORD,recordRect,recording?T(TX_FINISH):T(TX_RECORD),true,!playing,bodyBold,c);
    DrawButton(dc,C_PLAY,playRect,T(TX_PLAY),false,!recording&&!playing,bodyBold,c);
    DrawButton(dc,C_PAUSE,pauseRect,paused?(gLanguage?L"Continuar":L"Resume"):(gLanguage?L"Pausar":L"Pause"),false,recording||playing,bodyBold,c);
    DrawButton(dc,C_STOP,stopRect,T(TX_STOP),false,recording||playing,bodyBold,c);
    COLORREF statusColor = paused?c.muted:(recording?c.red:(playing?c.accent:(gStatus==ST_ERROR?c.red:c.green)));
    HBRUSH statusBrush=CreateSolidBrush(statusColor); auto oldStatus=SelectObject(dc,statusBrush); Ellipse(dc,controlsLeft,S(226),controlsLeft+S(9),S(235)); SelectObject(dc,oldStatus);DeleteObject(statusBrush);
    std::wstring status=CurrentStatusText();
    DrawString(dc,status.c_str(),{controlsLeft+S(16),S(216),controlsLeft+S(470),S(247)},bodyBold,statusColor);
    std::wstring hotkeySummary=HotkeySummary();DrawString(dc,hotkeySummary.c_str(),{controlsLeft,S(244),controlsLeft+S(504),S(265)},subtitleFont,c.muted);
    RECT settingsCard={S(28),S(298),width-S(28),height-S(74)};RoundFill(dc,settingsCard,16,c.surface,c.border);
    int mid=width/2;
    DrawString(dc,T(TX_LOOP_SETTINGS),{S(50),S(314),mid-S(20),S(337)},sectionFont,c.muted);
    DrawString(dc,T(TX_MODE),{S(50),S(346),S(155),S(370)},subtitleFont,c.muted);
    RECT modeRect={S(50),S(374),S(178),S(414)};DrawButton(dc,C_MODE,modeRect,gMode==0?T(TX_SINGLE):T(TX_ALTERNATE),false,!playing&&!recording,bodyBold,c);
    DrawString(dc,T(TX_REPETITIONS),{S(193),S(346),S(320),S(370)},subtitleFont,c.muted);
    RECT minusRect={S(193),S(374),S(229),S(414)},plusRect={S(302),S(374),S(338),S(414)};
    DrawButton(dc,C_REPEAT_MINUS,minusRect,L"−",false,!playing&&!gInfinite,bigFont,c);DrawButton(dc,C_REPEAT_PLUS,plusRect,L"+",false,!playing&&!gInfinite,bigFont,c);
    std::wstring repeats=Number(gRepeats);DrawString(dc,repeats.c_str(),{S(232),S(374),S(299),S(414)},bigFont,c.text,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    DrawString(dc,T(TX_INFINITE),{S(353),S(346),S(430),S(370)},subtitleFont,c.muted);RECT toggleRect={S(353),S(381),S(401),S(407)};DrawToggle(dc,toggleRect,gInfinite,c);AddHit(C_INFINITE,toggleRect,!playing);
    DrawString(dc,T(TX_SPEED),{S(416),S(346),S(520),S(370)},subtitleFont,c.muted);RECT speedMinus={S(416),S(374),S(448),S(414)},speedPlus={S(494),S(374),S(526),S(414)};DrawButton(dc,C_SPEED_MINUS,speedMinus,L"−",false,!playing,bigFont,c);DrawButton(dc,C_SPEED_PLUS,speedPlus,L"+",false,!playing,bigFont,c);std::wstring speed=SpeedLabel();DrawString(dc,speed.c_str(),{S(449),S(374),S(493),S(414)},bodyBold,c.text,DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    DrawString(dc,T(TX_PROFILE),{mid+S(20),S(314),width-S(50),S(337)},sectionFont,c.muted);
    RECT openRect={mid+S(20),S(350),mid+S(128),S(390)},saveRect={mid+S(138),S(350),mid+S(246),S(390)},clearRect={mid+S(256),S(350),mid+S(364),S(390)};
    DrawButton(dc,C_OPEN,openRect,T(TX_OPEN),false,!playing&&!recording,bodyBold,c);DrawButton(dc,C_SAVE,saveRect,T(TX_SAVE),true,!playing&&!recording,bodyBold,c);DrawButton(dc,C_CLEAR,clearRect,T(TX_CLEAR),false,!playing&&!recording,bodyBold,c);
    std::wstring profileLine=gCurrentProfile;DrawString(dc,profileLine.c_str(),{mid+S(22),S(397),width-S(52),S(424)},subtitleFont,c.muted);
    RECT hotkeysRect={mid+S(20),S(438),width-S(50),S(476)};std::wstring hotkeyLabel=std::wstring(gLanguage?L"⚙  Configurar atajos":L"⚙  Configure hotkeys")+L"  ·  "+HotkeySummary();DrawButton(dc,C_SETTINGS,hotkeysRect,hotkeyLabel.c_str(),false,!playing&&!recording,subtitleFont,c);

    std::wstring footer=(gMode==0?T(TX_SINGLE):T(TX_ALTERNATE));footer+=L"  ·  ";footer+=gInfinite?T(TX_INFINITE):(Number(gRepeats)+L" "+T(TX_CYCLES));
    DrawString(dc,footer.c_str(),{S(30),height-S(57),width-S(30),height-S(26)},subtitleFont,c.muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);

    }
    BitBlt(target,0,0,width,height,dc,0,0,SRCCOPY);
    DeleteObject(titleFont);DeleteObject(subtitleFont);DeleteObject(sectionFont);DeleteObject(bodyFont);DeleteObject(bodyBold);DeleteObject(bigFont);DeleteObject(metricFont);
    SelectObject(dc,oldBitmap);DeleteObject(bitmap);DeleteDC(dc);
}

static ControlId HitTestControl(POINT point, bool* enabled = nullptr) {
    for (const auto& hit : gHits) if (PtInRect(&hit.rect, point)) { if(enabled)*enabled=hit.enabled; return hit.id; }
    if(enabled)*enabled=false;return C_NONE;
}

static std::wstring FileNameOnly(const std::wstring& path) {
    size_t slash=path.find_last_of(L"\\/");std::wstring name=slash==std::wstring::npos?path:path.substr(slash+1);size_t dot=name.find_last_of(L'.');if(dot!=std::wstring::npos)name=name.substr(0,dot);return name;
}

static void ShowSaveDialog() {
    wchar_t path[MAX_PATH]=L"TaskFlow profile.taskflow";wchar_t filter[]=L"TaskFlow Profile (*.taskflow)\0*.taskflow\0All files (*.*)\0*.*\0\0";
    OPENFILENAMEW ofn={};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=gWindow;ofn.lpstrFilter=filter;ofn.lpstrFile=path;ofn.nMaxFile=MAX_PATH;ofn.lpstrDefExt=L"taskflow";ofn.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
    if(GetSaveFileNameW(&ofn)){if(SaveProfile(path)){gCurrentProfile=FileNameOnly(path);gStatus=ST_SAVED;}else gStatus=ST_ERROR;InvalidateRect(gWindow,nullptr,FALSE);}
}

static void ShowOpenDialog() {
    wchar_t path[MAX_PATH]={};wchar_t filter[]=L"TaskFlow Profile (*.taskflow)\0*.taskflow\0All files (*.*)\0*.*\0\0";
    OPENFILENAMEW ofn={};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=gWindow;ofn.lpstrFilter=filter;ofn.lpstrFile=path;ofn.nMaxFile=MAX_PATH;ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if(GetOpenFileNameW(&ofn)){if(LoadProfile(path)){gCurrentProfile=FileNameOnly(path);gStatus=ST_LOADED;SaveProfile(AutosavePath());}else gStatus=ST_ERROR;InvalidateRect(gWindow,nullptr,FALSE);}
}

static bool IsCompactWindow() {
    RECT client; GetClientRect(gWindow,&client); return client.right < S(760) || client.bottom < S(520);
}

static void OpenSettings() {
    if (gRecording || gPlaying) return;
    gShowSettings = true; gCaptureHotkey = -1;
    if (IsCompactWindow()) {
        GetWindowRect(gWindow,&gCompactRect); gRestoreCompactRect = true;
        int newWidth=S(880),newHeight=S(660);int centerX=(gCompactRect.left+gCompactRect.right)/2,centerY=(gCompactRect.top+gCompactRect.bottom)/2;
        SetWindowPos(gWindow,nullptr,centerX-newWidth/2,centerY-newHeight/2,newWidth,newHeight,SWP_NOZORDER|SWP_NOACTIVATE);
    }
    InvalidateRect(gWindow,nullptr,FALSE);
}

static void CloseSettings() {
    gShowSettings=false;gCaptureHotkey=-1;RegisterAppHotkeys();
    if(gRestoreCompactRect){SetWindowPos(gWindow,nullptr,gCompactRect.left,gCompactRect.top,gCompactRect.right-gCompactRect.left,gCompactRect.bottom-gCompactRect.top,SWP_NOZORDER|SWP_NOACTIVATE);gRestoreCompactRect=false;}
    InvalidateRect(gWindow,nullptr,FALSE);
}

static void EnterCompactMode() {
    if (gShowSettings) CloseSettings();
    RECT current; GetWindowRect(gWindow,&current);
    if (!IsCompactWindow()) { gExpandedRect=current; gHasExpandedRect=true; }
    int compactWidth=S(448), compactHeight=S(240);
    HMONITOR monitor=MonitorFromWindow(gWindow,MONITOR_DEFAULTTONEAREST);MONITORINFO info={sizeof(info)};GetMonitorInfoW(monitor,&info);
    int x=std::clamp(current.left,info.rcWork.left,info.rcWork.right-compactWidth);
    int y=std::clamp(current.top,info.rcWork.top,info.rcWork.bottom-compactHeight);
    gAutoCompact=true;
    SetWindowPos(gWindow,nullptr,x,y,compactWidth,compactHeight,SWP_NOZORDER|SWP_NOACTIVATE);
}

static void RestoreExpandedMode() {
    if (gHasExpandedRect) {
        RECT rect=gExpandedRect;gAutoCompact=false;
        SetWindowPos(gWindow,nullptr,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,SWP_NOZORDER|SWP_NOACTIVATE);
    } else {
        gAutoCompact=false;ShowWindow(gWindow,SW_MAXIMIZE);
    }
}

static void HandleControl(ControlId id) {
    bool recording=InterlockedCompareExchange(&gRecording,TRUE,TRUE),playing=InterlockedCompareExchange(&gPlaying,TRUE,TRUE);
    switch(id){
        case C_THEME:gDark=!gDark;ApplyTitlebarTheme();SaveSettings();break;
        case C_LANGUAGE:gLanguage=1-gLanguage;SaveSettings();break;
        case C_TAB_A:if(!recording&&!playing)gActiveTrack=0;SaveSettings();break;
        case C_TAB_B:if(!recording&&!playing)gActiveTrack=1;SaveSettings();break;
        case C_RECORD:if(!playing){if(recording)StopRecording();else StartRecording();}break;
        case C_PLAY:if(!recording&&!playing)StartPlayback();break;
        case C_PAUSE:TogglePause();break;
        case C_STOP:if(recording)StopRecording();if(playing)StopPlayback();break;
        case C_MODE:if(!recording&&!playing){gMode=1-gMode;SaveSettings();}break;
        case C_REPEAT_MINUS:if(!playing&&!gInfinite){gRepeats=std::max(1,gRepeats-1);SaveSettings();}break;
        case C_REPEAT_PLUS:if(!playing&&!gInfinite){gRepeats=std::min(999,gRepeats+1);SaveSettings();}break;
        case C_INFINITE:if(!playing){gInfinite=!gInfinite;SaveSettings();}break;
        case C_SPEED:if(!playing){gSpeedIndex=(gSpeedIndex+1)%10;SaveSettings();}break;
        case C_SPEED_MINUS:if(!playing){gSpeedIndex=std::max(0,gSpeedIndex-1);SaveSettings();}break;
        case C_SPEED_PLUS:if(!playing){gSpeedIndex=std::min(9,gSpeedIndex+1);SaveSettings();}break;
        case C_OPEN:if(!playing&&!recording)ShowOpenDialog();break;
        case C_SAVE:if(!playing&&!recording)ShowSaveDialog();break;
        case C_CLEAR:if(!playing&&!recording&&MessageBoxW(gWindow,T(TX_CONFIRM_CLEAR),T(TX_CONFIRM_TITLE),MB_YESNO|MB_ICONQUESTION)==IDYES){EnterCriticalSection(&gSequenceLock);gTracks[gActiveTrack].clear();gTrackStats[gActiveTrack]={};LeaveCriticalSection(&gSequenceLock);SaveProfile(AutosavePath());}break;
        case C_SETTINGS:OpenSettings();break;
        case C_SETTINGS_BACK:CloseSettings();break;
        case C_HOTKEY_RECORD:UnregisterHotKey(gWindow,HOTKEY_RECORD);UnregisterHotKey(gWindow,HOTKEY_PLAY);UnregisterHotKey(gWindow,HOTKEY_STOP);gCaptureHotkey=0;break;
        case C_HOTKEY_PLAY:UnregisterHotKey(gWindow,HOTKEY_RECORD);UnregisterHotKey(gWindow,HOTKEY_PLAY);UnregisterHotKey(gWindow,HOTKEY_STOP);gCaptureHotkey=1;break;
        case C_HOTKEY_STOP:UnregisterHotKey(gWindow,HOTKEY_RECORD);UnregisterHotKey(gWindow,HOTKEY_PLAY);UnregisterHotKey(gWindow,HOTKEY_STOP);gCaptureHotkey=2;break;
        case C_RESET_HOTKEYS:
            gHotkeys[0]={0,VK_F8};gHotkeys[1]={0,VK_F9};gHotkeys[2]={0,VK_F10};RegisterAppHotkeys();SaveSettings();gCaptureHotkey=-1;break;
        default:break;
    }
    InvalidateRect(gWindow,nullptr,FALSE);
}

static bool IsModifierKey(WPARAM key) {
    return key==VK_SHIFT||key==VK_CONTROL||key==VK_MENU||key==VK_LSHIFT||key==VK_RSHIFT||key==VK_LCONTROL||key==VK_RCONTROL||key==VK_LMENU||key==VK_RMENU||key==VK_LWIN||key==VK_RWIN;
}

static bool CaptureHotkey(WPARAM key) {
    if (!gShowSettings || gCaptureHotkey < 0) return false;
    if (key == VK_ESCAPE) { gCaptureHotkey=-1;RegisterAppHotkeys();InvalidateRect(gWindow,nullptr,FALSE);return true; }
    if (IsModifierKey(key)) return true;
    uint32_t modifiers=0;
    if(GetKeyState(VK_CONTROL)&0x8000)modifiers|=MOD_CONTROL;if(GetKeyState(VK_SHIFT)&0x8000)modifiers|=MOD_SHIFT;
    if(GetKeyState(VK_MENU)&0x8000)modifiers|=MOD_ALT;if((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000)modifiers|=MOD_WIN;
    AppHotkey old=gHotkeys[gCaptureHotkey];gHotkeys[gCaptureHotkey]={modifiers,(uint32_t)key};
    if(!RegisterAppHotkeys()){gHotkeys[gCaptureHotkey]=old;RegisterAppHotkeys();gStatus=ST_ERROR;MessageBoxW(gWindow,gLanguage?L"Ese atajo ya está siendo usado por otra aplicación.":L"That hotkey is already used by another application.",L"TaskFlow",MB_OK|MB_ICONWARNING);}else{SaveSettings();}
    gCaptureHotkey=-1;InvalidateRect(gWindow,nullptr,FALSE);return true;
}

static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch(message){
        case WM_CREATE:return 0;
        case WM_GETMINMAXINFO:{auto* info=(MINMAXINFO*)lParam;info->ptMinTrackSize.x=S(448);info->ptMinTrackSize.y=S(240);return 0;}
        case WM_DPICHANGED:{gDpi=HIWORD(wParam);RECT* suggested=(RECT*)lParam;SetWindowPos(window,nullptr,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
        case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);PaintWindow(dc);EndPaint(window,&ps);return 0;}
        case WM_ERASEBKGND:return 1;
        case WM_SYSCOMMAND:
            if((wParam&0xFFF0)==SC_MAXIMIZE){if(IsCompactWindow()||gAutoCompact)RestoreExpandedMode();else EnterCompactMode();return 0;}
            break;
        case WM_SIZE:if(wParam!=SIZE_MINIMIZED){RECT client;GetClientRect(window,&client);if(client.right>=S(760)&&client.bottom>=S(520))gAutoCompact=false;}InvalidateRect(window,nullptr,FALSE);return 0;
        case WM_MOUSEMOVE:{POINT p={GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};ControlId next=HitTestControl(p);if(next!=gHover){gHover=next;InvalidateRect(window,nullptr,FALSE);}TRACKMOUSEEVENT tme={sizeof(tme),TME_LEAVE,window,0};TrackMouseEvent(&tme);return 0;}
        case WM_MOUSELEAVE:gHover=C_NONE;InvalidateRect(window,nullptr,FALSE);return 0;
        case WM_LBUTTONUP:{POINT p={GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};bool enabled=false;ControlId id=HitTestControl(p,&enabled);if(enabled)HandleControl(id);return 0;}
        case WM_KEYDOWN:if(CaptureHotkey(wParam))return 0;if(gShowSettings&&wParam==VK_ESCAPE){CloseSettings();return 0;}break;
        case WM_SYSKEYDOWN:if(CaptureHotkey(wParam))return 0;break;
        case WM_HOTKEY:
            if(wParam==HOTKEY_RECORD&&!gPlaying){if(gRecording)StopRecording();else StartRecording();}
            else if(wParam==HOTKEY_PLAY){if(gPlaying||gRecording)TogglePause();else StartPlayback();}
            else if(wParam==HOTKEY_STOP){if(gRecording)StopRecording();if(gPlaying)StopPlayback();}
            return 0;
        case WM_APP_PLAYBACK_DONE:
            InterlockedExchange(&gPlaying,FALSE);InterlockedExchange(&gPaused,FALSE);if(gPlaybackThread){CloseHandle(gPlaybackThread);gPlaybackThread=nullptr;}gStatus=ST_STOPPED;InvalidateRect(window,nullptr,FALSE);return 0;
        case WM_CLOSE:
            if(gRecording)StopRecording();if(gPlaying){StopPlayback();if(gPlaybackThread)WaitForSingleObject(gPlaybackThread,2000);}SaveProfile(AutosavePath());SaveSettings();DestroyWindow(window);return 0;
        case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    gInstance=instance;InitializeCriticalSection(&gSequenceLock);QueryPerformanceFrequency(&gCounterFrequency);LoadSettings();LoadProfile(AutosavePath());
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSEXW wc={sizeof(wc)};wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=WindowProc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));wc.hIconSm=wc.hIcon;wc.lpszClassName=L"TaskFlowWindow";
    RegisterClassExW(&wc);
    gDpi=GetDpiForSystem();int width=MulDiv(1100,(int)gDpi,96),height=MulDiv(710,(int)gDpi,96);
    gWindow=CreateWindowExW(0,wc.lpszClassName,L"TaskFlow Pro",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,width,height,nullptr,nullptr,instance,nullptr);
    if(!gWindow){DeleteCriticalSection(&gSequenceLock);return 1;}
    gStopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);ApplyTitlebarTheme();RegisterAppHotkeys();
    ShowWindow(gWindow,showCommand);UpdateWindow(gWindow);
    MSG message;while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    UnregisterHotKey(gWindow,HOTKEY_RECORD);UnregisterHotKey(gWindow,HOTKEY_PLAY);UnregisterHotKey(gWindow,HOTKEY_STOP);
    if(gStopEvent)CloseHandle(gStopEvent);DeleteCriticalSection(&gSequenceLock);return (int)message.wParam;
}
