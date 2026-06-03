#include "controller.h"
#include "tweaks.h"

int main() {
    if (!ensurePermissions())
        return 1;

    auto window = MainWindow::create();
    Controller ctrl(*window);
    ctrl.load();
    window->run();
}
