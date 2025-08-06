#pragma once

#include <iostream>

#include "aabb.h"
#include "application.h"
#include "buffer.h"
#include "camera.h"
#include "camera_controller.h"
#include "drawable.h"
#include "image.h"
#include "primitive.h"
#include "shader.h"
#include "shapes.h"
#include "texture.h"

namespace fw {
template <typename App>
int RunApplication() {
  try {
    App app{};
    app.Run();
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Unhandled exception: " << e.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "Unhandled unknown exception\n";
    return 1;
  }
}
}  // namespace fw

#define APPLICATION_MAIN(AppType) /* NOLINT(cppcoreguidelines-macro-usage) */ \
  int main() { return ::fw::RunApplication<AppType>(); }
