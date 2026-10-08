// Entry point: builds the Application and runs its loop until the window closes.

#include "core/Application.hpp"

#include <filesystem>
#include <system_error>

int main(int, char** argv) {
    // The game reads assets/ (and writes its save) relative to the working
    // directory. Started from a shortcut or another folder, it moves to its
    // own folder first, so the packaged game runs however it is launched.
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists("assets", ec) && argv && argv[0]) {
        const auto home = fs::absolute(argv[0], ec).parent_path();
        if (!ec && fs::exists(home / "assets", ec)) fs::current_path(home, ec);
    }
    engine::Application app;
    app.run();
    return 0;
}
