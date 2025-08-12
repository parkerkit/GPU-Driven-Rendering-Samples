# Third-Party 第三方依赖

本目录用于统一管理 OpenGL 使用的第三方依赖。

项目采用 **Vendoring** 方式管理第三方库，即将经过适当裁剪的第三方源码直接保存在仓库中，而不是在 CMake Configure 阶段通过网络下载。

主要目标：

* 保证构建过程可复现；
* 构建时不依赖网络；
* 明确记录第三方库版本；
* 只保留实际需要的源码、头文件和许可证；
* 尽量保持第三方源码不修改；
* 避免第三方 Include Path 和编译配置污染整个工程；
* 所有第三方依赖统一通过 CMake Target 使用。

---

## CMake Target

`third_party/CMakeLists.txt` 对工程暴露以下 Target：

```text
ThirdParty::glad
ThirdParty::glfw
ThirdParty::glm
ThirdParty::imgui
ThirdParty::stb_image
ThirdParty::stb_image_write
ThirdParty::meshoptimizer
```

工程中的其他 Target 应通过：

```cmake
target_link_libraries(...)
```

声明第三方依赖。

例如：

```cmake
target_link_libraries(Framework
    PRIVATE
        ThirdParty::glfw
        ThirdParty::glad
        ThirdParty::imgui
        ThirdParty::stb_image
        ThirdParty::stb_image_write
        ThirdParty::meshoptimizer

    PUBLIC
        ThirdParty::glm
)
```

禁止通过全局：

```cmake
include_directories(...)
link_directories(...)
```

添加第三方库目录。

---

## 当前依赖

|库|版本|用途|构建方式|
|---|---|---|---|
|GLAD|GLAD2 Generated|OpenGL Function Loader|Static|
|GLFW|3.5.1|Window / Input / OpenGL Context|Static|
|GLM|1.0.3|数学库|Header-Only|
|Dear ImGui|1.92.9|Debug / Development UI|Static|
|stb_image|2.30|图片读取与解码|Static Wrapper|
|stb_image_write|1.16|图片编码与写入|Static Wrapper|
|meshoptimizer|1.2|优化3D网格模型|Static Warpper|

---

## GLAD

GLAD 使用 GLAD2 生成。

生成配置：

```text
API:
    OpenGL 4.6

Profile:
    Core

Extensions:
    All

Generator:
    C/C++

Options:
    alias       ON
    debug       OFF
    header only OFF
    loader      OFF
    merge       OFF
    mx          OFF
    on demand   OFF
```

GLAD Internal Loader 关闭。

OpenGL Function Address 由 GLFW：

```cpp
glfwGetProcAddress()
```

提供。

### 保留文件

```text
glad/
├── include/
│   ├── glad/
│   │   └── gl.h
│   └── KHR/
│       └── khrplatform.h
├── src/
│   └── gl.c
└── LICENSE
```

重新生成 GLAD 时，应保持相同配置，除非 Framework 的 OpenGL 最低版本或加载架构发生变化。

---

## GLFW

版本：

```text
3.5.1
```

上游：

```text
https://github.com/glfw/glfw
```

用途：

```text
Window
Input
OpenGL Context
Platform Abstraction
```

GLFW 使用其官方 CMake 构建系统，不手动维护不同平台的源码列表。

关闭：

```text
Examples
Tests
Documentation
Install Target
```

GLFW 强制构建为静态库。

### 保留内容

```text
glfw/
├── CMake/
├── deps/
├── include/
├── src/
├── CMakeLists.txt
└── LICENSE.md
```

其中：

```text
src/
```

保持完整，不根据当前开发平台删除：

```text
Win32
Cocoa
X11
Wayland
POSIX
Null Platform
EGL
OSMesa
```

相关源码。

这样可以保证 Framework 后续仍然可以在：

```text
Windows
Linux
macOS
```

之间迁移。

`deps/` 当前保持完整。

如果未来希望进一步裁剪，应确认 GLFW 3.5.1 的 CMake 构建系统不会引用被删除文件后再进行处理。

### 不保留

上游以下内容不属于 Framework 构建所需内容：

```text
.github/
docs/
examples/
tests/
```

以及其他 CI、Git 仓库元数据和开发辅助文件。

`LICENSE.md` 必须始终保留。

---

## GLM

版本：

```text
1.0.3
```

上游：

```text
https://github.com/g-truc/glm
```

GLM 作为 Header-Only Library 使用。

### 保留内容

```text
glm/
├── copying.txt
└── glm/
    └── [完整 GLM Header Tree]
```

整个：

```text
glm/glm/
```

头文件树保持完整。

不要根据当前代码实际 Include 的文件进一步删除 GLM 内部头文件，因为 GLM 存在较多内部和传递依赖。

### 不保留

例如：

```text
.github/
cmake/
doc/
test/
util/
CMakeLists.txt
manual.md
```

`copying.txt` 必须始终保留。

---

## Dear ImGui

版本：

```text
1.92.9
```

上游：

```text
https://github.com/ocornut/imgui
```

Framework 使用：

```text
Dear ImGui Core
+
GLFW Platform Backend
+
OpenGL3 Renderer Backend
```

### Core 保留文件

```text
imgui/
├── LICENSE.txt
├── imconfig.h
├── imgui.h
├── imgui_internal.h
│
├── imgui.cpp
├── imgui_draw.cpp
├── imgui_tables.cpp
├── imgui_widgets.cpp
│
├── imstb_rectpack.h
├── imstb_textedit.h
└── imstb_truetype.h
```

### Backend 保留文件

```text
imgui/backends/
├── imgui_impl_glfw.cpp
├── imgui_impl_glfw.h
├── imgui_impl_opengl3.cpp
├── imgui_impl_opengl3.h
└── imgui_impl_opengl3_loader.h
```

`imgui_impl_opengl3_loader.h` 不要删除。

ImGui OpenGL3 Backend 使用自己的精简 OpenGL Loader。

Framework 本身仍然使用 GLAD，两者互不冲突。

### imgui_demo.cpp

当前不编译：

```text
imgui_demo.cpp
```

Framework 开发阶段可以选择保留该文件作为 ImGui API 示例。

如果希望 third_party 保持最小化，也可以直接删除。

### 不保留

```text
.github/
docs/
examples/
misc/
```

以及所有没有使用的 Backend。

`LICENSE.txt` 必须始终保留。

---

## stb

上游：

```text
https://github.com/nothings/stb
```

本项目目前只使用：

```text
stb_image
stb_image_write
```

没有引入完整 stb 仓库。

当前对应版本：

```text
stb_image       2.30
stb_image_write 1.16
```

### 用途

#### stb_image

用于读取和解码常见图片格式，例如：

```text
JPEG
PNG
TGA
BMP
PSD
GIF
HDR
PIC
PNM
```

主要用于：

```text
Texture Loading
HDR Texture Loading
Image Asset Loading
```

#### stb_image_write

用于将内存中的图像数据写入文件。

主要用于：

```text
Screenshot
Framebuffer Capture
Debug Output
Generated Image Export
```

---

### stb 保留文件

只保留：

```text
stb/
├── stb_image.h
├── stb_image_write.h
└── LICENSE
```

不保留完整 stb Repository。

例如以下内容均不需要：

```text
.github/
data/
deprecated/
docs/
tests/
tools/

stb_image_resize2.h
stb_truetype.h
stb_rect_pack.h
stb_vorbis.c
...
```

如果未来需要其他 stb Library，再单独加入对应文件。

---

### stb Implementation

stb 的头文件默认只包含 API 声明。

实际实现必须在且仅在一个 Translation Unit 中定义对应 Implementation Macro。

为了避免修改上游头文件，Framework 在：

```text
third_party/src/
```

中维护自己的 Implementation Wrapper。

#### stb_image

```text
third_party/src/stb_image.cpp
```

内容：

```cpp
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
```

对应 CMake Target：

```text
ThirdParty::stb_image
```

#### stb_image_write

```text
third_party/src/stb_image_write.cpp
```

内容：

```cpp
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
```

对应 CMake Target：

```text
ThirdParty::stb_image_write
```

禁止在 Framework 或 Lesson 的其他 `.cpp` 文件中再次定义：

```text
STB_IMAGE_IMPLEMENTATION
STB_IMAGE_WRITE_IMPLEMENTATION
```

否则可能造成重复符号定义。

---

### stb 与未来 TinyGLTF 的关系

未来如果引入 TinyGLTF，需要特别注意 stb Implementation 的所有权。

本工程已经统一提供：

```text
ThirdParty::stb_image
ThirdParty::stb_image_write
```

因此 TinyGLTF 不应该再次自行生成另一份：

```text
STB_IMAGE_IMPLEMENTATION
STB_IMAGE_WRITE_IMPLEMENTATION
```

同一个第三方 Dependency 应尽量只存在一份实现。

原则：

```text
third_party/stb/
        │
        ├── ThirdParty::stb_image
        │
        └── ThirdParty::stb_image_write
                  ▲
                  │
          Framework / Asset Loader
```

而不是让多个第三方库分别维护自己的 stb Implementation。

---

## meshoptimizer

版本：

```text
v1.2
```

上游：

```text
https://github.com/zeux/meshoptimizer
```

用途：

```text
提升 GPU 渲染性能
极致压缩与快速传输
简化与 LOD 生成
```

meshoptimizer 使用 src 目录下的源码编译，不使用官方的 CMakeLists.txt

不编译：

```text
demo
extern
gltf
js
tools
```

meshoptimizer 强制构建为静态库。

### 保留内容

```text
meshoptimizer/
├── src/
└── LICENSE.md
```

其中：

```text
src/
```

保持完整，不删除任何源码。

### 不保留

上游以下内容不属于 Framework 构建所需内容：

```text
.github/
demo/
extern/
gltf/
js/
tools/
```

以及其他 CI、Git 仓库元数据和开发辅助文件。

`LICENSE.md` 必须始终保留。

---

## Third-Party Source 修改原则

原则上：

```text
不要修改第三方上游源码
```

如果必须增加工程集成代码，应优先添加到：

```text
third_party/src/
```

而不是修改：

```text
third_party/glfw/
third_party/glm/
third_party/imgui/
third_party/stb/
```

中的上游文件。

例如 stb 的 Implementation Wrapper 就位于：

```text
third_party/src/stb_image.cpp
third_party/src/stb_image_write.cpp
```

这样升级 stb 时，只需要替换：

```text
stb_image.h
stb_image_write.h
LICENSE
```

本地集成代码不会受到影响。

---

## 第三方库升级规则

升级第三方依赖时：

1. 获取明确的上游 Release、Tag 或 Commit。
2. 确认版本。
3. 阅读 Release Notes / Changelog。
4. 使用新版本文件完整替换旧版本对应文件。
5. 按当前规则删除不需要的目录和文件。
6. 不要混合新旧版本源码。
7. 不要修改上游源码，除非确有必要。
8. 更新本 README 中记录的版本。
9. 进行 Clean Build。
10. 在所有支持的平台上进行构建和基本运行测试。

对于没有统一 Release Version 的库，例如 stb，建议同时记录：

```text
Header Version
+
Upstream Commit
```

以保证未来能够准确定位使用的源码版本。

---

## 当前目录结构

```text
third_party/
├── CMakeLists.txt
├── README.md
│
├── src/
│   ├── stb_image.cpp
│   └── stb_image_write.cpp
│
├── glad/
│   ├── include/
│   ├── src/
│   └── LICENSE
│
├── glfw/
│   ├── CMake/
│   ├── deps/
│   ├── include/
│   ├── src/
│   ├── CMakeLists.txt
│   └── LICENSE.md
│
├── glm/
│   ├── copying.txt
│   └── glm/
│
├── imgui/
│   ├── backends/
│   ├── LICENSE.txt
│   ├── imconfig.h
│   ├── imgui.h
│   ├── imgui_internal.h
│   ├── imgui.cpp
│   ├── imgui_draw.cpp
│   ├── imgui_tables.cpp
│   ├── imgui_widgets.cpp
│   ├── imstb_rectpack.h
│   ├── imstb_textedit.h
│   └── imstb_truetype.h
│
├── stb/
│   ├── stb_image.h
│   ├── stb_image_write.h
│   └── LICENSE
│
└── meshoptimizer/
    ├── src/
    └── LICENSE.md
```
