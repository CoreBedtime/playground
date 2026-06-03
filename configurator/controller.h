#pragma once
#include "configurator.h"
#include "options.h"
#include "tweaks.h"

class Controller {
public:
    Controller(MainWindow& window);
    void load();

private:
    void save();
    void refreshTweaks();
    void openEditor(const std::string& name);
    void toggleTweak(const std::string& name);
    void packageTweak(const std::string& name);

    MainWindow& m_window;
    std::vector<TweakData> m_tweakInfos;
};
