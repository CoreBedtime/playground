#include "controller.h"
#include <algorithm>
#include <cstdio>
#include <memory>

Controller::Controller(MainWindow& window)
    : m_window(window)
{
    m_window.on_save([this] { save(); });
    m_window.on_edit_tweak([this](slint::SharedString name) { openEditor(std::string(name)); });
    m_window.on_toggle_tweak([this](slint::SharedString name) { toggleTweak(std::string(name)); });
    m_window.on_package_tweak([this](slint::SharedString name) { packageTweak(std::string(name)); });
}

void Controller::load() {
    Options opts = loadOptions();
    m_window.set_use_legacy_ammonia(opts.useLegacyAmmonia);
    m_window.set_disable_pac(opts.disablePAC);
    m_window.set_pause_injection(opts.pauseInjection);

    m_window.set_dev_tools_available(hasDeveloperTools());
    refreshTweaks();
}

void Controller::save() {
    Options opts;
    opts.useLegacyAmmonia = m_window.get_use_legacy_ammonia();
    opts.disablePAC = m_window.get_disable_pac();
    opts.pauseInjection = m_window.get_pause_injection();

    if (saveOptions(opts)) {
        m_window.set_status_message("Settings saved.");
    } else {
        m_window.set_status_message("Error: Cannot write to /opt/pluginplayground/current.options.");
    }
}

void Controller::refreshTweaks() {
    m_tweakInfos = scanTweaks();
    auto model = std::make_shared<slint::VectorModel<TweakInfo>>();
    auto defaultIcon = m_window.get_default_icon();
    for (const auto& t : m_tweakInfos) {
        TweakInfo ti;
        ti.name = slint::SharedString(t.name);
        ti.disabled = t.disabled;
        std::string iconPath = tweaksDir() + "/" + t.name + ".dylib.png";
        FILE* f = fopen(iconPath.c_str(), "rb");
        if (f) {
            fclose(f);
            ti.icon = slint::Image::load_from_path(slint::SharedString(iconPath.c_str()));
        } else {
            ti.icon = defaultIcon;
        }
        model->push_back(std::move(ti));
    }
    m_window.set_tweaks(model);
}

void Controller::openEditor(const std::string& name) {
    auto it = std::find_if(m_tweakInfos.begin(), m_tweakInfos.end(),
        [&](const TweakData& t) { return t.name == name; });
    if (it == m_tweakInfos.end()) return;

    auto editor = TweakEditor::create();
    editor->set_tweak_name(name.c_str());

    TweakOptions opts = loadTweakOptions(name);
    auto apps = std::make_shared<slint::VectorModel<slint::SharedString>>();
    for (const auto& a : opts.blacklistedApps)
        apps->push_back(slint::SharedString(a));
    editor->set_blacklisted_apps(apps);

    auto deps = std::make_shared<slint::VectorModel<slint::SharedString>>();
    for (const auto& d : opts.frameworkDependencies)
        deps->push_back(slint::SharedString(d));
    editor->set_framework_deps(deps);

    slint::ComponentWeakHandle<TweakEditor> weak(editor);

    editor->on_save([weak, name]() {
        auto opt = weak.lock();
        if (!opt) return;
        auto e = *opt;
        TweakOptions newOpts;
        auto appModel = e->get_blacklisted_apps();
        for (int i = 0; i < appModel->row_count(); i++)
            newOpts.blacklistedApps.push_back(std::string(*appModel->row_data(i)));
        auto depModel = e->get_framework_deps();
        for (int i = 0; i < depModel->row_count(); i++)
            newOpts.frameworkDependencies.push_back(std::string(*depModel->row_data(i)));
        saveTweakOptions(name, newOpts);
        e->hide();
    });

    editor->on_cancel([weak]() {
        auto opt = weak.lock();
        if (opt) (*opt)->hide();
    });

    editor->on_add_blacklisted([weak](slint::SharedString value) {
        auto opt = weak.lock();
        if (!opt) return;
        if (std::string(value).empty()) return;
        auto apps = (*opt)->get_blacklisted_apps();
        auto vec = std::dynamic_pointer_cast<slint::VectorModel<slint::SharedString>>(apps);
        if (vec) vec->push_back(value);
    });

    editor->on_add_framework_dep([weak](slint::SharedString value) {
        auto opt = weak.lock();
        if (!opt) return;
        if (std::string(value).empty()) return;
        auto deps = (*opt)->get_framework_deps();
        auto vec = std::dynamic_pointer_cast<slint::VectorModel<slint::SharedString>>(deps);
        if (vec) vec->push_back(value);
    });

    editor->on_remove_blacklisted([weak](slint::SharedString value) {
        auto opt = weak.lock();
        if (!opt) return;
        auto apps = (*opt)->get_blacklisted_apps();
        auto vec = std::dynamic_pointer_cast<slint::VectorModel<slint::SharedString>>(apps);
        if (!vec) return;
        for (int i = 0; i < vec->row_count(); i++) {
            if (*vec->row_data(i) == value) {
                vec->erase(i);
                break;
            }
        }
    });

    editor->on_remove_framework_dep([weak](slint::SharedString value) {
        auto opt = weak.lock();
        if (!opt) return;
        auto deps = (*opt)->get_framework_deps();
        auto vec = std::dynamic_pointer_cast<slint::VectorModel<slint::SharedString>>(deps);
        if (!vec) return;
        for (int i = 0; i < vec->row_count(); i++) {
            if (*vec->row_data(i) == value) {
                vec->erase(i);
                break;
            }
        }
    });

    editor->show();
}

void Controller::toggleTweak(const std::string& name) {
    auto it = std::find_if(m_tweakInfos.begin(), m_tweakInfos.end(),
        [&](const TweakData& t) { return t.name == name; });
    if (it == m_tweakInfos.end()) return;

    if (::toggleTweak(name)) {
        it->disabled = !it->disabled;
        refreshTweaks();
    } else {
        m_window.set_status_message("Error: Cannot toggle " + slint::SharedString(name));
    }
}

void Controller::packageTweak(const std::string& name) {
    if (::packageTweak(name)) {
        m_window.set_status_message((std::string("Packaged: /tmp/") + name + ".pkg").c_str());
    } else {
        m_window.set_status_message((std::string("Package failed for ") + name).c_str());
    }
}
