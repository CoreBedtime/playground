#pragma once

struct Options {
    bool useLegacyAmmonia = false;
    bool disablePAC = false;
    bool pauseInjection = false;
};

Options loadOptions();
bool saveOptions(const Options& opts);
