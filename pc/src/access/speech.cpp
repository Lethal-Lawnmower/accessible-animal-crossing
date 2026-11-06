// speech.cpp - Prism-backed speech output. See access/speech.h.
//
// Prism calls happen only on the worker thread, so a slow screen reader or
// speech engine can never stall a game frame.

#include "access/speech.h"

#include <prism.h>

#include <borealis/log.hpp>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace {

constexpr borealis::Log Log{"access::speech"};

struct Request {
    enum class Kind { Speak, Stop } kind;
    std::string text;
    bool interrupt;
};

std::mutex g_mutex;
std::condition_variable g_wake;
std::deque<Request> g_queue;
std::thread g_worker;
bool g_quit = false;
bool g_started = false;
std::atomic<bool> g_available{false};
std::atomic<bool> g_logged_unavailable{false};

// Last interrupting announcement, for "repeat". Guarded by g_mutex.
std::string g_last;

void worker_main() {
    PrismConfig config = prism_config_init();
    PrismContext* context = prism_init(&config);
    PrismBackend* backend = context ? prism_registry_acquire_best(context) : nullptr;

    if (!context) {
        Log.error("BLOCKER: Prism failed to initialize. No speech this session.");
    } else if (!backend) {
        Log.error("BLOCKER: Prism found no usable speech output. No speech this session.");
    } else {
        Log.info("Prism speech output: {}", prism_backend_name(backend));
        g_available = true;
    }

    std::unique_lock lock(g_mutex);
    for (;;) {
        g_wake.wait(lock, [] { return g_quit || !g_queue.empty(); });
        if (g_quit) {
            break;
        }
        Request request = std::move(g_queue.front());
        g_queue.pop_front();
        lock.unlock();

        if (backend) {
            PrismError error = PRISM_OK;
            if (request.kind == Request::Kind::Stop) {
                error = prism_backend_stop(backend);
            } else {
                // output = speech plus braille where the screen reader supports it.
                error = prism_backend_output(backend, request.text.c_str(), request.interrupt);
            }
            if (error != PRISM_OK) {
                Log.warn("Prism request failed: {}", prism_error_string(error));
            }
        }

        lock.lock();
    }
    lock.unlock();

    if (backend) {
        prism_backend_free(backend);
    }
    if (context) {
        prism_shutdown(context);
    }
    g_available = false;
}

void submit(Request request) {
    {
        std::lock_guard lock(g_mutex);
        if (!g_started) {
            if (!g_logged_unavailable.exchange(true)) {
                Log.warn("Speech requested before the speech module started; dropped");
            }
            return;
        }
        if (request.interrupt || request.kind == Request::Kind::Stop) {
            g_queue.clear();
        }
        g_queue.push_back(std::move(request));
    }
    g_wake.notify_one();
}

} // namespace

extern "C" {

void acc_speech_init(void) {
    std::lock_guard lock(g_mutex);
    if (g_started) {
        return;
    }
    g_quit = false;
    g_started = true;
    g_worker = std::thread(worker_main);
}

void acc_speech_shutdown(void) {
    {
        std::lock_guard lock(g_mutex);
        if (!g_started) {
            return;
        }
        g_quit = true;
        g_started = false;
        g_queue.clear();
    }
    g_wake.notify_one();
    if (g_worker.joinable()) {
        g_worker.join();
    }
}

bool acc_speech_available(void) {
    return g_available;
}

void acc_speak(const char* utf8, bool interrupt) {
    if (!utf8 || !*utf8) {
        return;
    }
    Log.debug("{} {}", interrupt ? "say!" : "say ", utf8);
    if (interrupt) {
        std::lock_guard lock(g_mutex);
        g_last = utf8;
    }
    submit({Request::Kind::Speak, utf8, interrupt});
}

void acc_speak_interrupt(const char* utf8) {
    acc_speak(utf8, true);
}

void acc_speak_queue(const char* utf8) {
    acc_speak(utf8, false);
}

void acc_speech_repeat(void) {
    std::string last;
    {
        std::lock_guard lock(g_mutex);
        last = g_last;
    }
    if (!last.empty()) {
        Log.debug("repeat {}", last);
        submit({Request::Kind::Speak, std::move(last), true});
    }
}

void acc_speech_stop(void) {
    submit({Request::Kind::Stop, {}, true});
}

} // extern "C"
