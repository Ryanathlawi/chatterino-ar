<div dir="rtl">

<p align="center"><img src="docs/logo-ar.png" width="140" alt="شاترينو العربي"></p>

# شاترينو العربي

برنامج شات تويتش **بالعربي**، مبني على [Chatterino](https://github.com/Chatterino/chatterino2) المعروف، بس معرّب بالكامل، وبتصميم جديد، وبمميزات مخصوصة لنا.

**Made By RayanAthlawi**

![لقطة من البرنامج](docs/screenshot-ar.png)

---

## وش يميّزه؟

- **كل شي بالعربي:** القوائم والإعدادات والرسائل، والاتجاه من اليمين لليسار.
- **الشات مرتب:** الرسالة العربية تبدأ من اليمين، والإنجليزية من اليسار، وكل وحدة تنقرا صح.
- **صور المتابعين:** صورة كل شخص تطلع جنب اسمه في الشات.
- **تشوف البث فوق الشات:** مشغّل صغير بجودة خفيفة، ويبدأ بدون صوت.
- **تنحسب مشاهد:** وأنت في الشات، تنحسب مشاهد في البث بدون ما يثقل جهازك.
- **تصميم داكن مريح:** وكل الإعدادات تفتح في نفس النافذة.

---

## طريقة التحميل والتشغيل (خطوة بخطوة)

**١. ادخل صفحة التحميل**
افتح هذا الرابط: [صفحة الإصدارات](../../releases/latest)

**٢. نزّل الملف**
تحت كلمة **Assets**، اضغط على الملف اللي اسمه `Chatterino-AR-windows-x64.zip` وانتظر لين يخلص التحميل.

**٣. فك الضغط**
روح لمجلد التنزيلات، واضغط على الملف بالزر اليمين، واختر **Extract All** (أو «استخراج الكل»)، وبعدين اضغط **Extract**.

**٤. شغّل البرنامج**
افتح المجلد اللي طلع، وشغّل ملف **`chatterino.exe`** (اللي عليه الشعار الأخضر).

> **إذا طلعت لك رسالة زرقاء من ويندوز (Windows protected your PC):**
> هذا طبيعي لأن البرنامج جديد. اضغط **More info** وبعدين **Run anyway**.

**٥. (اختياري) خله على سطح المكتب**
اضغط على `chatterino.exe` بالزر اليمين ← **Send to** ← **Desktop (create shortcut)**.

> ⚠️ لا تنقل ملف `chatterino.exe` لحاله برا المجلد، لأنه يحتاج الملفات اللي معه. انقل المجلد كامل إذا تبي.

---

## أول ما تفتحه

**١. سجّل دخولك بحساب تويتش**
اضغط على أيقونة الترس ⚙️ فوق ← **الحسابات** ← **إضافة** ← **تسجيل الدخول**، وكمّل التسجيل في المتصفح.
(بدون تسجيل دخول تقدر تقرا الشات، بس ما تقدر تكتب، وما تطلع صور المتابعين.)

**٢. افتح قناة**
اضغط **إضافة قناة** تحت يمين الشاشة، واكتب اسم القناة، واضغط **موافق**.

**٣. البث والصوت**
إذا القناة لايف، البث بيطلع فوق الشات تلقائيًا:
- **تشغيل الصوت:** يشغّل الصوت، واضغطه مرة ثانية للكتم.
- **إخفاء البث:** يسكّر المشغّل لهذي القناة.

---

## الإعدادات المهمة

كلها في: الترس ⚙️ ← **عام**

| الخيار | وش يسوي |
|---|---|
| **احسبني مشاهد في البث** | يحسبك مشاهد في القنوات اللي قدامك، بالخلفية وبدون ما ياكل رامات. |
| **عرض البث فوق الشات** | يطلع المشغّل الصغير فوق الشات. لو تبي البرنامج أخف، طفّه. |

---

## أسئلة متكررة

**البرنامج ثقيل على جهازي؟**
طفّ **عرض البث فوق الشات** من الإعدادات، وبيصير خفيف جدًا.

**البث ما يطلع؟**
تأكد إن القناة لايف. إذا لايف ولسا ما طلع، انتظر دقيقة، لأنه يشيّك على القنوات كل دقيقة.

**صور المتابعين ما تطلع؟**
لازم تكون مسجّل دخول بحساب تويتش.

**أبي أحدّثه لنسخة جديدة؟**
نزّل آخر نسخة من [صفحة الإصدارات](../../releases/latest) وبدّل المجلد القديم بالجديد. إعداداتك وحسابك ما تضيع، لأنها محفوظة في جهازك برا المجلد.

---

## للمبرمجين: البناء من الكود

1. ثبّت **Visual Studio 2022** (Build Tools، مع C++)، و **Qt 6.8.3** (msvc2022_64 ومعه Qt Multimedia)، و **Python** و **Conan**.
2. حمّل الكود من هذا المستودع.
3. شغّل `build-ar.bat`، والبرنامج يطلع في `build\bin\chatterino.exe`.

**عن الخط:** النسخة المنشورة هنا تستخدم خط **IBM Plex Sans Arabic** (رخصته مفتوحة OFL). إذا عندك ملفات خط ثمانية من [موقعهم الرسمي](https://thmanyah.com)، حطها في `resources/fonts/` وابنِ البرنامج، وبيستخدمها تلقائيًا بداله.

---

## الرخص

- الكود تحت رخصة **MIT** نفسها حق Chatterino (شوف [LICENSE](LICENSE)).
- خط IBM Plex Sans Arabic تحت رخصة **SIL OFL 1.1** (شوف `resources/fonts/OFL-IBMPlexSansArabic.txt`).
- مشغّل البث يستخدم Qt Multimedia و FFmpeg (رخصة **LGPL**).

</div>

---

![chatterinoLogo](https://user-images.githubusercontent.com/41973452/272541622-52457e89-5f16-4c83-93e7-91866c25b606.png)
Chatterino 2 [![GitHub Actions Build (Windows, Ubuntu, MacOS)](https://github.com/Chatterino/chatterino2/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/Chatterino/chatterino2/actions?query=workflow%3ABuild+branch%3Amaster) [![Cirrus CI Build (FreeBSD only)](https://api.cirrus-ci.com/github/Chatterino/chatterino2.svg?branch=master)](https://cirrus-ci.com/github/Chatterino/chatterino2/master) [![Chocolatey Package](https://img.shields.io/chocolatey/v/chatterino?include_prereleases)](https://chocolatey.org/packages/chatterino) [![Flatpak Package](https://img.shields.io/flathub/v/com.chatterino.chatterino)](https://flathub.org/apps/details/com.chatterino.chatterino)
============

Chatterino 2 is a chat client for [Twitch.tv](https://twitch.tv).
The Chatterino 2 wiki can be found [here](https://wiki.chatterino.com).
Contribution guidelines can be found [here](https://wiki.chatterino.com/Contributing%20for%20Developers).

## Download

Current releases are available at [https://chatterino.com](https://chatterino.com).
Windows users can also install Chatterino [from Chocolatey](https://chocolatey.org/packages/chatterino).

## Nightly build

You can download the latest Chatterino 2 build over [here](https://github.com/Chatterino/chatterino2/releases/tag/nightly-build)

You might also need to install the [VC++ Redistributables](https://aka.ms/vs/17/release/vc_redist.x64.exe) from Microsoft if you do not have it installed already.  
If you still receive an error about `MSVCR120.dll missing`, then you should install the [VC++ 2013 Restributable](https://download.microsoft.com/download/2/E/6/2E61CFA4-993B-4DD4-91DA-3737CD5CD6E3/vcredist_x64.exe).

## Building

To get source code with required submodules run:

```shell
git clone --recurse-submodules https://github.com/Chatterino/chatterino2.git
```

or

```shell
git clone https://github.com/Chatterino/chatterino2.git
cd chatterino2
git submodule update --init --recursive
```

- [Building on Windows](../master/BUILDING_ON_WINDOWS.md)
- [Building on Windows with vcpkg](../master/BUILDING_ON_WINDOWS_WITH_VCPKG.md)
- [Building on Linux](../master/BUILDING_ON_LINUX.md)
- [Building on macOS](../master/BUILDING_ON_MAC.md)
- [Building on FreeBSD](../master/BUILDING_ON_FREEBSD.md)

## Git blame

This project has big commits in the history which touch most files while only doing stylistic changes. To improve the output of git-blame, consider setting:

```shell
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

This will ignore all revisions mentioned in the [`.git-blame-ignore-revs`
file](./.git-blame-ignore-revs). GitHub does this by default.

## Code style

The code is formatted using [clang-format](https://clang.llvm.org/docs/ClangFormat.html). Our configuration is found in the [.clang-format](.clang-format) file in the repository root directory.

For more contribution guidelines, take a look at [the wiki](https://wiki.chatterino.com/Contributing%20for%20Developers/).

## Doxygen

Doxygen is used to generate project information daily and is available [here](https://doxygen.chatterino.com).
