#include "Engine/Application.h"

int main() {
    Hex::AppSpecification spec;
    spec.name = "Sandbox";
    spec.width = 800;
    spec.height = 600;
    spec.fullscreen = false;
    spec.vsync = false;

    const auto application = Hex::Application(spec);
    application.Run();

    return 0;
}