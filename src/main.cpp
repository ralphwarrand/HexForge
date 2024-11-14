#include "Engine/Application.h"

int main() {
    Hex::AppSpecification spec;
    spec.name = "Sandbox";
    spec.width = 1920;
    spec.height = 1080;
    spec.fullscreen = false;

    const auto application = Hex::Application(spec);
    application.Run();

    return 0;
}
