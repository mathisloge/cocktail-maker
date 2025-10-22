# 🍸 Cocktail Maker

**Cocktail Maker** is a modern **Qt-based desktop application** for creating and managing cocktail recipes — and even controlling real-world hardware to mix your drinks automatically!

---

## 🧭 Overview

This project combines **C++23**, **Qt**, and a range of modern C++ libraries to deliver a sleek, responsive, and extensible cocktail-making experience.

---

## 🎥 Demo

Watch a short demo on YouTube:
👉 [https://youtu.be/46x27AaOHKw](https://youtu.be/46x27AaOHKw)

[![Watch the demo](https://img.youtube.com/vi/46x27AaOHKw/0.jpg)](https://www.youtube.com/watch?v=46x27AaOHKw)

---

## 🧩 Features

- 🥃 Choose cocktail recipes
- 🧠 Safe and type-checked units with [mp-units](https://github.com/mpusz/mp-units)
- ⚡ Asynchronous hardware control using `boost::asio`
- 💡 GPIO control with libgpiod
- 💬 Fast and structured logging via [spdlog](https://github.com/gabime/spdlog)
- 🎨 Responsive UI built with [Qt](https://qt.io)
- 🧪 Comprehensive testing using [catch2](https://github.com/catchorg/Catch2) and [quite](https://github.com/mathisloge/quite/) for UI tests

---

## 📚 Contribute Recipes

Want to expand the cocktail collection?
Add your recipes here:
➡️ [recipes directory](https://github.com/mathisloge/cocktail-maker/tree/main/ui/db/src/recipes)

---

## 🛠️ Technologies Used

| Purpose | Library / Framework |
|----------|--------------------|
| Language | **C++23** |
| UI | [Qt](https://qt.io) |
| Async I/O | **Boost.Asio** |
| GPIO Control | **libgpiod** |
| Units & Safety | [mp-units](https://github.com/mpusz/mp-units) |
| Logging | [spdlog](https://github.com/gabime/spdlog) |
| Formatting | [fmt](https://github.com/fmtlib/fmt) |
| Testing | [Catch2](https://github.com/catchorg/Catch2), [quite](https://github.com/mathisloge/quite/) |

---

## 🚀 Getting Started

### Clone the repository

1. Download [vcpkg](https://github.com/microsoft/vcpkg) and set the environment variable `VCPKG_ROOT`.

Then:
```bash
git clone https://github.com/mathisloge/cocktail-maker.git
cd cocktail-maker
cmake --workflow production
```

Afterwards install the generated .deb package and run `cocktail-maker`.


### Hardware setup

**TBD**
