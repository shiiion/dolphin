# Building PrimeHack on modern macOS (Xcode 26+ / CMake 4+ / Qt 6.11+)

Verified working (clean build + clean launch) on 2026-09-05 with Xcode 26.6,
CMake 4.4.3, and Homebrew's Qt 6.11.2 on Apple Silicon. **No source changes
needed** — just a couple of extra Homebrew packages and some configure flags
that the plain `cmake ..` from the section above doesn't know about.

---

## 🇪🇸 Español

### Requisitos previos
1. Instalá Xcode desde la App Store, abrilo una vez y aceptá los términos.
2. Apuntá `xcode-select` a Xcode:
   ```sh
   sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
   ```
3. Instalá Homebrew: https://brew.sh
4. Instalá CMake y Qt6 (con soporte SVG):
   ```sh
   brew install cmake qtbase qtsvg
   ```

### Clonar y preparar submódulos
```sh
git clone https://github.com/<tu-usuario>/dolphin.git
cd dolphin
git submodule update --init --recursive
```

### Configurar con CMake
```sh
mkdir build && cd build
cmake \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DQT_DIR="/opt/homebrew/opt/qtbase/lib/cmake/Qt6" \
  -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qtbase;/opt/homebrew/opt/qtsvg" \
  -DQt6QMacStylePlugin_DIR="/opt/homebrew/opt/qtbase/lib/cmake/Qt6Widgets" \
  -DQt6QCocoaIntegrationPlugin_DIR="/opt/homebrew/opt/qtbase/lib/cmake/Qt6Gui" \
  -DQt6Svg_DIR="/opt/homebrew/opt/qtsvg/lib/cmake/Qt6Svg" \
  -DENABLE_VULKAN=OFF \
  -DSKIP_POSTPROCESS_BUNDLE=ON \
  ..
```

¿Para qué sirve cada flag?
- **`CMAKE_POLICY_VERSION_MINIMUM=3.5`**: CMake 4 eliminó la compatibilidad con
  declaraciones `cmake_minimum_required` muy viejas, usadas por algunas
  librerías incluidas como submódulo. Sin esto, la configuración falla.
- **`QT_DIR` / `CMAKE_PREFIX_PATH` / `Qt6QMacStylePlugin_DIR` /
  `Qt6QCocoaIntegrationPlugin_DIR` / `Qt6Svg_DIR`**: apuntan explícitamente al
  Qt6 de Homebrew (incluyendo el módulo SVG, que CMake no encuentra solo). En
  un Mac Intel, la ruta suele ser `/usr/local/opt/...` en vez de
  `/opt/homebrew/opt/...`.
- **`ENABLE_VULKAN=OFF`**: evita compilar el MoltenVK embebido (no se probó
  a fondo contra el SDK más nuevo de Xcode). No hace falta para jugar: macOS
  ya tiene un backend nativo de **Metal** además de OpenGL.
- **`SKIP_POSTPROCESS_BUNDLE=ON`**: salta el paso que empaqueta todas las
  librerías dentro del `.app` para redistribución. No hace falta para uso
  personal/local.

Nota: a diferencia de guías más viejas para este proyecto, **no hace falta**
tocar `MBEDTLS_FATAL_WARNINGS` — eso ya no es un problema en el código actual.

### Compilar e instalar
```sh
make -j$(sysctl -n hw.ncpu)
cp -R Binaries/Dolphin.app /Applications/PrimeHack.app
```
Usá el nombre que prefieras. Si ya tenés el Dolphin normal instalado, no lo
sobreescribas.

### Probado con
Xcode 26.6, CMake 4.4.3, Homebrew `qtbase` + `qtsvg` 6.11.2, macOS Tahoe,
Apple Silicon (arm64). ~55 minutos desde el clone hasta un `.app` funcionando,
sin tocar código fuente.

---

## 🇬🇧 English

### Prerequisites
1. Install Xcode from the App Store, open it once and accept the terms.
2. Point `xcode-select` at Xcode:
   ```sh
   sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
   ```
3. Install Homebrew: https://brew.sh
4. Install CMake and Qt6 (with SVG support):
   ```sh
   brew install cmake qtbase qtsvg
   ```

### Clone and init submodules
```sh
git clone https://github.com/<your-username>/dolphin.git
cd dolphin
git submodule update --init --recursive
```

### Configure with CMake
```sh
mkdir build && cd build
cmake \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DQT_DIR="/opt/homebrew/opt/qtbase/lib/cmake/Qt6" \
  -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qtbase;/opt/homebrew/opt/qtsvg" \
  -DQt6QMacStylePlugin_DIR="/opt/homebrew/opt/qtbase/lib/cmake/Qt6Widgets" \
  -DQt6QCocoaIntegrationPlugin_DIR="/opt/homebrew/opt/qtbase/lib/cmake/Qt6Gui" \
  -DQt6Svg_DIR="/opt/homebrew/opt/qtsvg/lib/cmake/Qt6Svg" \
  -DENABLE_VULKAN=OFF \
  -DSKIP_POSTPROCESS_BUNDLE=ON \
  ..
```

What each flag does:
- **`CMAKE_POLICY_VERSION_MINIMUM=3.5`**: CMake 4 dropped support for very old
  `cmake_minimum_required` declarations used by some bundled submodules.
  Without this, configuring fails outright.
- **`QT_DIR` / `CMAKE_PREFIX_PATH` / `Qt6QMacStylePlugin_DIR` /
  `Qt6QCocoaIntegrationPlugin_DIR` / `Qt6Svg_DIR`**: explicitly point at
  Homebrew's Qt6 (including the SVG module, which CMake won't find on its
  own). On an Intel Mac the path is usually `/usr/local/opt/...` instead of
  `/opt/homebrew/opt/...`.
- **`ENABLE_VULKAN=OFF`**: skips building the bundled MoltenVK (not
  thoroughly tested against the newest Xcode SDK). Not needed to play — macOS
  already ships a native **Metal** backend in addition to OpenGL.
- **`SKIP_POSTPROCESS_BUNDLE=ON`**: skips the step that bundles every dylib
  into a redistributable `.app`. Not needed for a personal/local build.

Note: unlike older guides for this project, **`MBEDTLS_FATAL_WARNINGS` does
not need to be touched** — that's no longer an issue in the current code.

### Build and install
```sh
make -j$(sysctl -n hw.ncpu)
cp -R Binaries/Dolphin.app /Applications/PrimeHack.app
```
Use whatever name you like. If you already have vanilla Dolphin installed,
don't overwrite it.

### Tested with
Xcode 26.6, CMake 4.4.3, Homebrew `qtbase` + `qtsvg` 6.11.2, macOS Tahoe,
Apple Silicon (arm64). ~55 minutes from clone to a working `.app`, with zero
source changes.
