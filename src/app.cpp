#include "app.h"

namespace rune {

App& app() {
    static App instance;
    return instance;
}

} // namespace rune
