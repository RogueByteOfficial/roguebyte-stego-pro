# RogueByte Stego Pro — روج بايت ستيغو برو

[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/Build-CMake%203.20%2B-064F8C.svg)](https://cmake.org)
[![Security](https://img.shields.io/badge/AEAD-AES--256--GCM-green.svg)](https://www.openssl.org)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platforms](https://img.shields.io/badge/Platforms-Linux%20%7C%20Kali%20%7C%20Windows%20%7C%20Termux-purple.svg)](#الأنظمة-المدعومة)

**RogueByte Stego Pro** هي أداة سطر أوامر متقدمة وعالية الأداء، صُممت بالكامل بلغة **C++20** لإخفاء البيانات (ستيغانوغرافيا) والتحقيق الجنائي الرقمي عبر مختلف أنظمة التشغيل. تتيح الأداة تضمين أي ملف ثنائي في صور غطاء مدعومة مع تشفير محكم بالمصادقة (AEAD) وفحص جنائي إحصائي دقيق.

- **المطور**: SidoDev-Dz
- **المؤسسة**: RogueByte
- **اللغة الأساسية**: C++20

---

## المزايا الرئيسية

1. **دعم أي حمولة ثنائية**:
   - تضمين ملفات مضغوطة (ZIP)، ومستندات PDF، وملفات نصية وMarkdown، وبرمجيات تنفيذية (EXE, DLL)، وحزم تطبيقات (APK)، وشفرات مصدرية.
   - معالجة الحمولات كبيانات ثنائية مجردة غير موثوقة وعدم تنفيذها برمجياً تحت أي ظرف.
   - حفظ أسماء الملفات، وأحجامها، وتواريخ التعديل، وبصمات SHA-256 داخل ترويسة الأرشيف المتعدد (`RBM1`).

2. **محركات صور مستقلة وعديمة الفقدان**:
   - **PNG**: تضمين عديم الفقدان على قنوات ألوان RGB عبر استبدال البت الأقل أهمية (LSB)، مع الحفاظ التام على الشفافية (Alpha).
   - **BMP**: تضمين عديم الفقدان على صور 24 و32 بت النقطية مع المعالجة الدقيقة لحشو أسطر البكسلات.
   - **JPEG**: تضمين قطاعي بنيوي موثق داخل قطاعات العلامات المعتمدة (APP15 / `RBS_STEG`) دون إعادة ضغط أو تشويه معاملات DCT.
   - حساب السعة المتاحة مسبقاً ورفض أي حمولة تتجاوز سعة الغطاء مع حماية الملفات الأصلية من أي تعديل.

3. **تشفير متطور وهندسة حاويات آمنة**:
   - **تشفير معتمد بالمصادقة**: AES-256-GCM.
   - **اشتقاق المفاتيح**: PBKDF2-HMAC-SHA256 مع 100,000 تكرار وملح عشوائي بطول 256 بت.
   - **مقاومة التلاعب**: أي تغيير في البتات يؤدي إلى فشل فوري في المصادقة.
   - **ترويسة الحاوية الموثقة (`RBS1`)**: حجم ثابت 112 بايت، مع تصفير للذاكرة الحساسة عبر `OPENSSL_cleanse`.

4. **محرك التحقيق الجنائي الرقمي**:
   - فحص سلامة وتنسيق الصور.
   - قياس إنتروبيا شانون (Shannon Entropy) لجميع مستويات البتات الثمانية.
   - اختبار كاي-تربيع لتوزيع أزواج القيم (Chi-Square PoV) لاكتشاف الشذوذ الإحصائي.
   - اكتشاف ترويسات RogueByte بدقة ودون الحاجة لكلمة المرور.
   - توضيح المحددات الجنائية والتمييز بين النتائج المؤكدة والاستدلالات الاحتمالية.

5. **معايير أمان مشددة**:
   - كتابة ذرية آمنة للملفات المستخرجة والمخرجة عبر ملفات تبديل مؤقتة.
   - حماية كاملة ضد هجمات تخطي المسارات (`../` أو المسارات المطلقة).
   - حصر استخراج الملفات بدقة داخل المجلد المحدد.

---

## الأنظمة المدعومة

- **Linux x86_64** (Ubuntu, Debian, Fedora, Arch)
- **Kali Linux**
- **Android Termux** (باستخدام حزمة Clang، ودون الحاجة لصلاحيات Root)
- **Windows 10/11 x64** (عبر MSVC أو MinGW)

---

## خطوات التثبيت والبناء

### أنظمة Linux وKali Linux

```bash
# تثبيت الحزم المطلوبة
sudo apt-get update && sudo apt-get install -y build-essential cmake libssl-dev

# استنساخ المستودع والبناء
git clone https://github.com/SidoDev-Dz/roguebyte-stego-pro.git
cd roguebyte-stego-pro
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# تشغيل الاختبارات
ctest --test-dir build --output-on-failure
```

### تطبيق Termux على أندرويد (دون روت)

```bash
pkg update && pkg install -y clang cmake make openssl
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

---

## أوامر الاستخدام

```bash
# 1. قياس السعة المتاحة لصورة الغطاء
./build/rb-stego capacity --cover cover.png

# 2. تضمين وتشفير الحمولات
./build/rb-stego embed --cover cover.png --input secret.zip --output result.png --encrypt

# 3. التحليل الجنائي للصورة
./build/rb-stego inspect --input result.png

# 4. استخراج الملفات المخفية
./build/rb-stego extract --input result.png --output-dir ./recovered/

# 5. التحقق من سلامة البصمة التشفيرية SHA-256
./build/rb-stego verify --input ./recovered/secret.zip

# 6. عرض معلومات الإصدار
./build/rb-stego version
```

---

## الترخيص

تم تطوير المشروع بواسطة **SidoDev-Dz** تحت مؤسسة **RogueByte**. مرخص بموجب رخصة **MIT**.
