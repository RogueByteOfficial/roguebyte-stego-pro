# دليل البناء والتثبيت متعدد المنصات

**المطور**: SidoDev-Dz | **المؤسسة**: RogueByte

## المتطلبات الأساسية

- مترجم C++ متوافق مع معيار C++20 (GCC 11+ أو Clang 13+ أو MSVC 2022).
- نظام CMake بالإصدار 3.20 أو أحدث.
- مكتبة OpenSSL 3.0 مع ملفات الترويسة التطويرية (`libssl-dev`).

---

## 1. التثبيت على Linux وKali Linux

```bash
# تثبيت الحزم المطلوبة:
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev

# إعداد وبناء المشروع:
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# تشغيل حزمة الاختبارات:
ctest --test-dir build --output-on-failure
```

---

## 2. التثبيت على أندرويد عبر Termux (دون روت)

```bash
# داخل تطبيق Termux:
pkg update
pkg install -y clang cmake make openssl

# البناء باستخدام مترجم Clang:
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
cmake --build build -j$(nproc)

# التحقق من الاختبارات:
ctest --test-dir build --output-on-failure
```

---

## 3. التثبيت على Windows 10/11 x64

```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
