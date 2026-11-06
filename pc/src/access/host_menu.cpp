// host_menu.cpp - Speech for the port's RmlUi menus. See access/host_menu.h.
//
// The port builds its menus with borealis::ui on RmlUi. Controls such as
// select buttons are elements with child <key> (label) and <value> elements
// holding plain text; icons are CSS classes on an <icon> element and carry no
// text. So the readable content of a control is the text of its visible
// descendants. A "modified" value is prefixed by the UI with a bullet, which
// is dropped here.

#include "access/host_menu.h"

#include "access/speech.h"

#include <aurora/rmlui.hpp>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementText.h>
#include <borealis/log.hpp>

#include <string>

namespace {

constexpr borealis::Log Log{"access::host_menu"};

Rml::Element* s_last_focus = nullptr;
std::string s_last_text;
std::string s_last_value;
std::string s_last_help;

void append_text(Rml::Element* element, std::string& out) {
    if (!element || !element->IsVisible()) {
        return;
    }
    if (auto* text = dynamic_cast<Rml::ElementText*>(element)) {
        const Rml::String& t = text->GetText();
        if (!t.empty()) {
            if (!out.empty() && out.back() != ' ') {
                out += ' ';
            }
            out += t;
        }
        return;
    }
    for (int i = 0; i < element->GetNumChildren(); i++) {
        append_text(element->GetChild(i), out);
    }
}

// Remove the UI's "modified" bullet and non-breaking spaces, trim.
std::string clean(std::string text) {
    const std::string bullet = "\xE2\x80\xA2";  // •
    const std::string nbsp = "\xC2\xA0";
    for (size_t p; (p = text.find(bullet)) != std::string::npos;) {
        text.erase(p, bullet.size());
    }
    for (size_t p; (p = text.find(nbsp)) != std::string::npos;) {
        text.replace(p, nbsp.size(), " ");
    }
    for (size_t p; (p = text.find("  ")) != std::string::npos;) {
        text.erase(p, 1);
    }
    size_t start = text.find_first_not_of(' ');
    size_t end = text.find_last_not_of(' ');
    if (start == std::string::npos) {
        return {};
    }
    return text.substr(start, end - start + 1);
}

// First descendant with the given tag name (depth-first).
Rml::Element* find_child_tag(Rml::Element* element, const char* tag) {
    for (int i = 0; i < element->GetNumChildren(); i++) {
        Rml::Element* child = element->GetChild(i);
        if (child->GetTagName() == tag) {
            return child;
        }
        if (Rml::Element* found = find_child_tag(child, tag)) {
            return found;
        }
    }
    return nullptr;
}

std::string describe(Rml::Element* focus, std::string* value_out) {
    std::string all;
    append_text(focus, all);
    all = clean(all);

    std::string value;
    if (Rml::Element* value_elem = find_child_tag(focus, "value")) {
        append_text(value_elem, value);
        value = clean(value);
    }
    *value_out = value;

    if (Rml::Element* key_elem = find_child_tag(focus, "key")) {
        std::string key;
        append_text(key_elem, key);
        key = clean(key);
        if (!key.empty() && !value.empty()) {
            all = key + ": " + value;
        }
    }
    if (focus->IsPseudoClassSet("disabled")) {
        all += all.empty() ? "unavailable" : ", unavailable";
    }
    return all;
}

// Settings-style windows show help for the focused control in a pane to the
// right of the pane holding the control (borealis Pane::register_control
// refills it on focus). Read the panes that follow the focused one.
std::string help_text(Rml::Element* focus) {
    Rml::Element* pane = focus;
    while (pane && pane->GetTagName() != "pane") {
        pane = pane->GetParentNode();
    }
    if (!pane || !pane->GetParentNode()) {
        return {};
    }
    Rml::Element* parent = pane->GetParentNode();
    std::string text;
    bool after = false;
    for (int i = 0; i < parent->GetNumChildren(); i++) {
        Rml::Element* child = parent->GetChild(i);
        if (child == pane) {
            after = true;
            continue;
        }
        if (after && child->GetTagName() == "pane") {
            append_text(child, text);
        }
    }
    return clean(text);
}

} // namespace

void acc_host_menu_update() {
    Rml::Context* context = aurora::rmlui::get_context();
    Rml::Element* focus = context ? context->GetFocusElement() : nullptr;
    // Hidden documents can still hold focus: the menu bar is pushed hidden
    // at startup with its first tab ("Settings") focused, which was spoken
    // before the title screen. Check the ancestors too.
    if (!focus || focus == context->GetRootElement() || !focus->IsVisible(true)) {
        s_last_focus = nullptr;
        s_last_text.clear();
        s_last_value.clear();
        s_last_help.clear();
        return;
    }

    std::string value;
    std::string text = describe(focus, &value);
    if (focus != s_last_focus) {
        Log.debug("focus <{}> id '{}'", focus->GetTagName(), focus->GetId());
        if (!text.empty()) {
            acc_speak_interrupt(text.c_str());
        }
        std::string help = help_text(focus);
        if (!help.empty() && help != s_last_help) {
            acc_speak_queue(help.c_str());
        }
        s_last_help = help;
    } else if (text != s_last_text) {
        // Same control, new content: usually a changed setting.
        const std::string& say = (!value.empty() && value != s_last_value) ? value : text;
        if (!say.empty()) {
            acc_speak_interrupt(say.c_str());
        }
    }
    s_last_focus = focus;
    s_last_text = text;
    s_last_value = value;
}
