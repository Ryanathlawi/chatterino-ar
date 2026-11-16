# Arabic UI: replaces exact English string literals with Arabic, per file.
# Run from repo root: python tools/ar/translate.py   (idempotent)
# ponytail: literal replacement, no Qt i18n; switch to tr()+.ts if upstream merges become painful.
import pathlib, sys

T = {
"src/widgets/Window.cpp": {
    "Window": "النافذة", "Minimize": "تصغير", "Select next tab": "التبويب التالي",
    "Select previous tab": "التبويب السابق", "Help": "مساعدة",
},
"src/widgets/Notebook.cpp": {
    "Lock Tab Layout": "قفل ترتيب التبويبات", "Top most window": "النافذة دائمًا في المقدمة",
    "Chatterino - hidden tabs": "Chatterino - التبويبات مخفية",
    "You've just hidden your tabs.": "أخفيت التبويبات للتو.",
    "Don't show again": "لا تظهر مرة أخرى", "Show all tabs": "إظهار كل التبويبات",
    "Only show live tabs": "إظهار القنوات المباشرة فقط", "Hide all tabs": "إخفاء كل التبويبات",
    "Sort Tabs Alphabetically": "ترتيب التبويبات أبجديًا", "Tab visibility": "ظهور التبويبات",
},
"src/widgets/splits/SplitHeader.cpp": {
    "Add split": "إضافة قسم", "Vod-casting": "إعادة بث", "Live": "مباشر",
    "Change channel": "تغيير القناة", "Close": "إغلاق", "Popup": "نافذة منفصلة",
    "Popup overlay": "نافذة طافية", "Search": "بحث", "Set filters": "تعيين الفلاتر",
    "Open in custom player": "فتح في مشغّل مخصص", "Create a clip": "إنشاء مقطع",
    "Reconnect": "إعادة الاتصال", "Reload channel emotes": "إعادة تحميل إيموتات القناة",
    "Reload subscriber emotes": "إعادة تحميل إيموتات المشتركين", "How to...": "كيف أسوّي...",
    "More": "المزيد", "Toggle moderation mode": "تبديل وضع الإشراف",
    "Show chatter list": "قائمة المتواجدين", "Subscribe": "اشتراك",
    "Notify when live": "نبّهني إذا بدأ البث", "Mute highlight sounds": "كتم أصوات التمييز",
    "Clear messages": "مسح الرسائل", "Show changelog": "سجل التغييرات",
    "Subscriber only": "للمشتركين فقط", "Emote only": "إيموتات فقط", "Slow": "بطيء",
    "Followers only": "للمتابعين فقط", "Seconds:": "الثواني:", "Time:": "المدة:",
},
"src/widgets/splits/SplitInput.cpp": {
    "Check spelling": "التدقيق الإملائي", "Replying to @": "رد على @",
},
"src/widgets/splits/Split.cpp": {
    "Image upload": "رفع صورة", "Yes, don't ask again": "نعم، لا تسألني مرة أخرى",
    "Log in to send messages...": "سجّل دخولك لإرسال الرسائل...",
    "Send message as %1...": "أرسل رسالة باسم %1...", "Change channel": "تغيير القناة",
    "Moving": "النقل", "Splitting": "التقسيم", "Select filters": "اختر الفلاتر",
    "An error occurred trying to process your image: %1": "صار خطأ أثناء معالجة الصورة: %1",
},
"src/widgets/helper/ChannelView.cpp": {
    "Open link &incognito": "فتح الرابط في وضع &التصفح الخفي",
    "Copy message &ID": "نسخ &معرّف الرسالة", "Copy message &JSON": "نسخ الرسالة بصيغة &JSON",
    "More messages below": "رسائل أكثر بالأسفل", "Paused": "متوقف مؤقتًا",
    "Copy &message": "نسخ ال&رسالة", "Copy &full message": "نسخ الرسالة &كاملة",
    "Reply to &original thread": "الرد على ال&محادثة الأصلية", "View &thread": "عرض ال&محادثة",
    "Mo&derate": "إ&شراف", "Open in new &tab": "فتح في &تبويب جديد",
    "Open player in &browser": "فتح المشغّل في المت&صفح", "Open in &streamlink": "فتح في &streamlink",
    "Open in custom &player": "فتح في م&شغّل مخصص",
},
"src/widgets/dialogs/SelectChannelDialog.cpp": {
    "Select a channel to join": "اختر قناة للانضمام", "Channel": "قناة",
    "Join a Twitch channel by its channel name": "انضم لقناة Twitch باسمها",
    "Whispers": "الهمسات", "Shows the whispers that you receive while Chatterino is running":
    "يعرض الهمسات اللي توصلك وChatterino شغّال", "Mentions": "الإشارات",
    "Shows all the messages that highlight you from any channel": "يعرض كل الرسائل اللي تذكرك من أي قناة",
    "Watching": "المشاهدة الحالية", "Requires the Chatterino browser extension": "يتطلب إضافة Chatterino للمتصفح",
    "Live": "مباشر", "Shows when channels go live": "يعرض متى تبدأ القنوات البث",
},
"src/widgets/dialogs/SettingsDialog.cpp": {
    "Chatterino Settings": "إعدادات Chatterino", "Find in settings... ": "ابحث في الإعدادات... ",
    "Cancel": "إلغاء", "General": "عام", "Accounts": "الحسابات", "Nicknames": "الألقاب",
    "Commands": "الأوامر", "Highlights": "التمييز", "Ignores": "التجاهل", "Filters": "الفلاتر",
    "Hotkeys": "الاختصارات", "Moderation": "الإشراف", "Live Notifications": "تنبيهات البث",
    "External tools": "أدوات خارجية", "Plugins": "الإضافات", "About": "حول",
    "Streamer Mode": "وضع الستريمر",
},
}

root = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / "tools" / "ar"))
import json
from extract import ui_strings
from settings_ar import AR

# Settings pages: only label/tooltip/title arguments (see extract.is_ui),
# adjacent literals joined, so stored setting values are never touched.
EN = json.loads((root / "tools/ar/settings_en.json").read_text(encoding="utf-8"))
assert len(EN) == len(AR), (len(EN), len(AR))
UI = dict(zip(EN, AR))
for p in sorted((root / "src/widgets/settingspages").glob("*.cpp")):
    s = p.read_text(encoding="utf-8")
    for a, b, t in reversed(ui_strings(s)):
        if t in UI:
            s = s[:a] + '"' + UI[t] + '"' + s[b:]
    p.write_text(s, encoding="utf-8")
missing = []
for rel, pairs in T.items():
    p = root / rel
    s = p.read_text(encoding="utf-8")
    for en, ar in pairs.items():
        lit = '"' + en + '"'
        if lit in s:
            s = s.replace(lit, '"' + ar + '"')
        elif ('"' + ar + '"') not in s:
            missing.append(f"{rel}: {en}")
    p.write_text(s, encoding="utf-8")
# Commands + user card: exact joined-literal match; skip log/debug lines.
import re
from extract import sequences
import cmd_ar
hits = set()
for pat in cmd_ar.FILES:
    for p in sorted(root.glob(pat)):
        s = p.read_text(encoding="utf-8")
        card = p.name == "UserInfoPopup.cpp"
        for a, b, t in reversed(list(sequences(s))):
            ar = cmd_ar.AR.get(t)
            if ar is None or (" " not in t and not card):
                continue
            line = s[s.rfind(chr(10), 0, a) + 1:a]
            if re.search(r"<<|qC(Debug|Warning|Info|Critical)|assert", line):
                continue
            s = s[:a] + '"' + ar + '"' + s[b:]
            hits.add(t)
        p.write_text(s, encoding="utf-8")
# Rest of the UI: display arguments only (is_ui_wide), plus a few exact assignments.
import rest_ar
from extract import is_ui_wide
for pat in rest_ar.FILES:
    for p in sorted(root.glob(pat)):
        s = p.read_text(encoding="utf-8")
        for a, b, t in reversed(list(sequences(s))):
            ar = rest_ar.AR.get(t)
            if ar is None or not is_ui_wide(s, a):
                continue
            s = s[:a] + '"' + ar + '"' + s[b:]
        p.write_text(s, encoding="utf-8")
for rel, pairs in rest_ar.EXTRA.items():
    p = root / rel
    s = p.read_text(encoding="utf-8")
    for en, ar in pairs.items():
        s = s.replace('"' + en + '"', '"' + ar + '"')
    p.write_text(s, encoding="utf-8")

print("not found:", *missing, sep="\n  ") if missing else print("ok")
sys.exit(1 if missing else 0)
