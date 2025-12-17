
## Aquamarine (Shift fork)

**aquamarine-shift** is a fork of Aquamarine adapted for use by [**Shift**](https://github.com/hyprside/shift) and **Hyprside**.

It extends Aquamarine with a **Tab backend**, allowing [hyprde-wm](https://github.com/hyprside/hyprde-wm) to connect to shift without having to change almost anything in the original hyprland code.

This fork is **not compatible with upstream Aquamarine users** and is **not intended as a general-purpose replacement**.

---

## What is Aquamarine?

Aquamarine is a very light Linux rendering backend library. It provides basic abstractions
for an application to render on:

- a Wayland session (in a window)
- a native DRM session

It is agnostic of the rendering API (Vulkan / OpenGL) and is designed to be lightweight,
performant, and minimal.

Aquamarine provides **no bindings for other languages**. It is **C++-only**.

---

## What this fork adds

This fork introduces functionality required by Shift:

### TabBackend
- A backend built around **tab_client**
- Designed for **system-compositor-style rendering**, not desktop compositing
- Rendering is driven by **buffer availability**, not Wayland frame callbacks
- Supports direct DMABUF-based presentation

---

## Intended usage

This fork is intended to be used **only** by:

- **Shift**
- **Hyprside**
- Projects explicitly designed around Shift’s architecture

If you are building a traditional Wayland compositor or desktop environment,
**you should use upstream Aquamarine instead**.

---

## Stability

Aquamarine depends on the ABI stability of the C++ standard library implementation
used by your compiler.

SO version bumps will occur only for **Aquamarine ABI breaks**, not stdlib changes.

The Shift fork does **not** guarantee ABI stability relative to upstream Aquamarine, in fact it changes some internal interfaces which make it completely incompatible with upstream straight up.

---

## Building

```sh
cmake --no-warn-unused-cli \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -S . -B ./build

cmake --build ./build --config Release \
  --target all \
  -j$(nproc 2>/dev/null || getconf _NPROCESSORS_CONF)
````

---

## Backend support

* [x] Wayland backend
* [x] DRM backend (DRM / KMS / libinput)
* [x] Virtual backend (headless)
* [x] **Tab backend (Shift / Hyprside)**
* [ ] Hardware plane support

---

## Relationship to upstream

This repository is a **fork** of:

[https://github.com/hyprwm/aquamarine](https://github.com/hyprwm/aquamarine)

Upstream Aquamarine intentionally does **not** support the assumptions made by Shift.
Changes in this fork may diverge significantly and are not expected to be upstreamed.
