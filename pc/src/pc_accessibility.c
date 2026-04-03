/* pc_accessibility.c - Tolk screen reader integration via runtime DLL loading.
 *
 * Loads Tolk.dll dynamically so the game runs fine without it — accessibility
 * features simply become no-ops if no screen reader is present. All Tolk
 * functions use wchar_t (UTF-16); we convert from UTF-8 internally. */

#include "pc_accessibility.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#else
/* Tolk is Windows-only. Provide stubs on other platforms. */
#endif

/* ========================================================================= */
/* Tolk function pointer types (mirrors Tolk.h)                              */
/* ========================================================================= */
#ifdef _WIN32

typedef void    (__cdecl *pfn_Tolk_Load)(void);
typedef bool    (__cdecl *pfn_Tolk_IsLoaded)(void);
typedef void    (__cdecl *pfn_Tolk_Unload)(void);
typedef void    (__cdecl *pfn_Tolk_TrySAPI)(bool trySAPI);
typedef void    (__cdecl *pfn_Tolk_PreferSAPI)(bool preferSAPI);
typedef const wchar_t* (__cdecl *pfn_Tolk_DetectScreenReader)(void);
typedef bool    (__cdecl *pfn_Tolk_HasSpeech)(void);
typedef bool    (__cdecl *pfn_Tolk_HasBraille)(void);
typedef bool    (__cdecl *pfn_Tolk_Output)(const wchar_t* str, bool interrupt);
typedef bool    (__cdecl *pfn_Tolk_Speak)(const wchar_t* str, bool interrupt);
typedef bool    (__cdecl *pfn_Tolk_Braille)(const wchar_t* str);
typedef bool    (__cdecl *pfn_Tolk_IsSpeaking)(void);
typedef bool    (__cdecl *pfn_Tolk_Silence)(void);

static HMODULE              s_tolk_dll          = NULL;
static pfn_Tolk_Load        s_Tolk_Load         = NULL;
static pfn_Tolk_IsLoaded    s_Tolk_IsLoaded     = NULL;
static pfn_Tolk_Unload      s_Tolk_Unload       = NULL;
static pfn_Tolk_TrySAPI     s_Tolk_TrySAPI      = NULL;
static pfn_Tolk_PreferSAPI  s_Tolk_PreferSAPI   = NULL;
static pfn_Tolk_DetectScreenReader s_Tolk_DetectScreenReader = NULL;
static pfn_Tolk_HasSpeech   s_Tolk_HasSpeech    = NULL;
static pfn_Tolk_HasBraille  s_Tolk_HasBraille   = NULL;
static pfn_Tolk_Output      s_Tolk_Output       = NULL;
static pfn_Tolk_Speak       s_Tolk_Speak        = NULL;
static pfn_Tolk_Braille     s_Tolk_Braille      = NULL;
static pfn_Tolk_IsSpeaking  s_Tolk_IsSpeaking   = NULL;
static pfn_Tolk_Silence     s_Tolk_Silence      = NULL;

static bool s_loaded = false;
static bool s_active = false; /* screen reader detected */

/* Last spoken text for repeat functionality */
#define ACC_LAST_TEXT_SIZE 512
static char s_last_text[ACC_LAST_TEXT_SIZE] = {0};

/* ========================================================================= */
/* UTF-8 → wchar_t conversion                                               */
/* ========================================================================= */
static wchar_t* utf8_to_wchar(const char* str) {
    if (!str) return NULL;
    int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (len <= 0) return NULL;
    wchar_t* buf = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (!buf) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, str, -1, buf, len);
    return buf;
}

/* ========================================================================= */
/* Load a single function pointer from the DLL                               */
/* ========================================================================= */
#define LOAD_FUNC(name) \
    s_##name = (pfn_##name)GetProcAddress(s_tolk_dll, #name); \
    if (!s_##name) { \
        fprintf(stderr, "[Accessibility] Missing function: %s\n", #name); \
        FreeLibrary(s_tolk_dll); \
        s_tolk_dll = NULL; \
        return; \
    }

void pc_acc_init(void) {
    if (s_loaded) return;

    s_tolk_dll = LoadLibraryA("Tolk.dll");
    if (!s_tolk_dll) {
        fprintf(stderr, "[Accessibility] Tolk.dll not found — TTS disabled.\n");
        fprintf(stderr, "[Accessibility] Place Tolk.dll next to AnimalCrossing.exe to enable.\n");
        return;
    }

    LOAD_FUNC(Tolk_Load);
    LOAD_FUNC(Tolk_IsLoaded);
    LOAD_FUNC(Tolk_Unload);
    LOAD_FUNC(Tolk_TrySAPI);
    LOAD_FUNC(Tolk_PreferSAPI);
    LOAD_FUNC(Tolk_DetectScreenReader);
    LOAD_FUNC(Tolk_HasSpeech);
    LOAD_FUNC(Tolk_HasBraille);
    LOAD_FUNC(Tolk_Output);
    LOAD_FUNC(Tolk_Speak);
    LOAD_FUNC(Tolk_Braille);
    LOAD_FUNC(Tolk_IsSpeaking);
    LOAD_FUNC(Tolk_Silence);

    /* Enable SAPI as fallback if no screen reader is running */
    s_Tolk_TrySAPI(true);
    s_Tolk_PreferSAPI(false);

    s_Tolk_Load();
    s_loaded = true;

    const wchar_t* sr = s_Tolk_DetectScreenReader();
    if (sr) {
        s_active = true;
        fprintf(stderr, "[Accessibility] Screen reader detected: %ls\n", sr);
    } else if (s_Tolk_HasSpeech()) {
        /* SAPI fallback active, no dedicated screen reader */
        s_active = true;
        fprintf(stderr, "[Accessibility] No screen reader found, using SAPI fallback.\n");
    } else {
        fprintf(stderr, "[Accessibility] No screen reader or SAPI available.\n");
    }
}

#undef LOAD_FUNC

void pc_acc_shutdown(void) {
    if (s_loaded && s_Tolk_Unload) {
        s_Tolk_Unload();
    }
    if (s_tolk_dll) {
        FreeLibrary(s_tolk_dll);
        s_tolk_dll = NULL;
    }
    s_loaded = false;
    s_active = false;
}

bool pc_acc_is_active(void) {
    return s_active;
}

void pc_acc_speak(const char* str, bool interrupt) {
    if (!s_active || !str || !s_Tolk_Speak) return;

    /* Store for repeat functionality */
    strncpy(s_last_text, str, ACC_LAST_TEXT_SIZE - 1);
    s_last_text[ACC_LAST_TEXT_SIZE - 1] = '\0';

    wchar_t* wstr = utf8_to_wchar(str);
    if (wstr) {
        s_Tolk_Speak(wstr, interrupt);
        /* Also send to braille display if one is connected */
        if (s_Tolk_HasBraille && s_Tolk_HasBraille() && s_Tolk_Braille) {
            s_Tolk_Braille(wstr);
        }
        free(wstr);
    }
}

void pc_acc_repeat_last(void) {
    if (!s_active || !s_Tolk_Speak) return;
    if (s_last_text[0] == '\0') return;
    wchar_t* wstr = utf8_to_wchar(s_last_text);
    if (wstr) {
        s_Tolk_Speak(wstr, true);
        if (s_Tolk_HasBraille && s_Tolk_HasBraille() && s_Tolk_Braille) {
            s_Tolk_Braille(wstr);
        }
        free(wstr);
    }
}

void pc_acc_speak_queue(const char* str) {
    pc_acc_speak(str, false);
}

void pc_acc_speak_interrupt(const char* str) {
    pc_acc_speak(str, true);
}

void pc_acc_braille(const char* str) {
    if (!s_active || !str || !s_Tolk_Braille) return;
    wchar_t* wstr = utf8_to_wchar(str);
    if (wstr) {
        s_Tolk_Braille(wstr);
        free(wstr);
    }
}

void pc_acc_output(const char* str, bool interrupt) {
    if (!s_active || !str || !s_Tolk_Output) return;
    wchar_t* wstr = utf8_to_wchar(str);
    if (wstr) {
        s_Tolk_Output(wstr, interrupt);
        free(wstr);
    }
}

void pc_acc_silence(void) {
    if (!s_active || !s_Tolk_Silence) return;
    s_Tolk_Silence();
}

bool pc_acc_is_speaking(void) {
    if (!s_active || !s_Tolk_IsSpeaking) return false;
    return s_Tolk_IsSpeaking();
}

#else
/* ========================================================================= */
/* Non-Windows stubs                                                         */
/* ========================================================================= */
void pc_acc_init(void) {}
void pc_acc_shutdown(void) {}
bool pc_acc_is_active(void) { return false; }
void pc_acc_speak(const char* str, bool interrupt) { (void)str; (void)interrupt; }
void pc_acc_speak_queue(const char* str) { (void)str; }
void pc_acc_speak_interrupt(const char* str) { (void)str; }
void pc_acc_braille(const char* str) { (void)str; }
void pc_acc_output(const char* str, bool interrupt) { (void)str; (void)interrupt; }
void pc_acc_silence(void) {}
bool pc_acc_is_speaking(void) { return false; }
void pc_acc_repeat_last(void) {}
#endif
