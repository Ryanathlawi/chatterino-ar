// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ExternalToolsPage.hpp"

#include "controllers/spellcheck/SpellChecker.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "util/Clipboard.hpp"
#include "util/Helpers.hpp"
#include "util/ImageUploader.hpp"
#include "util/StreamLink.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

#include <algorithm>

namespace chatterino {

namespace {

inline const QStringList STREAMLINK_QUALITY = {
    "Choose", "Source", "High", "Medium", "Low", "Audio only",
};

void exportImageUploaderSettings(QWidget *parent)
{
    const auto &s = *getSettings();

    QJsonObject settingsObj = imageuploader::detail::exportSettings(s);
    QJsonDocument doc(settingsObj);
    crossPlatformCopy(doc.toJson(QJsonDocument::Indented));

    QMessageBox::information(
        parent, "تم تصدير الإعدادات",
        "تم نسخ إعدادات رافع الصور إلى الحافظة بصيغة JSON.");
}

void importImageUploaderSettings(QWidget *parent)
{
    QString clipboardText = getClipboardText().trimmed();

    auto res = imageuploader::detail::validateImportJson(clipboardText);
    if (!res)
    {
        QMessageBox::warning(
            parent, "فشل الاستيراد",
            QString("خطأ في التحقق من إعدادات رافع الصور المستوردة: %1.")
                .arg(res.error()));
        return;
    }
    const auto &settingsObj = *res;

    int ret = QMessageBox::question(
        parent, "استيراد الإعدادات",
        "هذا بيستبدل إعدادات رافع الصور الحالية. تكمل؟",
        QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes)
    {
        return;
    }

    auto &s = *getSettings();
    if (imageuploader::detail::importSettings(settingsObj, s))
    {
        QMessageBox::information(
            parent, "تم الاستيراد بنجاح",
            "تم استيراد إعدادات رافع الصور بنجاح!");
    }
    else
    {
        QMessageBox::warning(
            parent, "فشل الاستيراد",
            "ما لقينا إعدادات صالحة لرافع الصور في JSON.");
    }
}

}  // namespace

ExternalToolsPage::ExternalToolsPage()
    : view(GeneralPageView::withoutNavigation(this))
{
    auto *y = new QVBoxLayout;
    auto *x = new QHBoxLayout;
    x->addWidget(this->view);
    auto *z = new QFrame;
    z->setLayout(x);
    y->addWidget(z);
    this->setLayout(y);

    this->initLayout(*this->view);
}

bool ExternalToolsPage::filterElements(const QString &query)
{
    if (this->view)
    {
        return this->view->filterElements(query) || query.isEmpty();
    }

    return false;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void ExternalToolsPage::initLayout(GeneralPageView &layout)
{
    auto &s = *getSettings();

    {
        auto *form = new QFormLayout;
        layout.addTitle("Streamlink");
        layout.addDescription("Streamlink أداة سطر أوامر تمرّر البث من خدمات مختلفة إلى مشغّل فيديو مثل VLC. تأكد إنك تعدّل ملف الإعدادات قبل لا تستخدمه!");
        layout.addDescription(
            formatRichNamedLink("https://streamlink.github.io/", "Website") +
            " " +
            formatRichNamedLink(
                "https://github.com/streamlink/streamlink/releases/latest",
                "Download") +
            " " +
            formatRichNamedLink("https://streamlink.github.io/cli.html#twitch",
                                "Documentation"));

        SettingWidget::checkbox("استخدام مسار مخصص (فعّله إذا Streamlink مثبّت في مسار غير المعتاد)",
                                s.streamlinkUseCustomPath)
            ->addTo(layout);

        layout.addDescription(
            QStringLiteral(
                "Chatterino expects the executable to be called \"%1\".")
                .arg(STREAMLINK_BINARY_NAME));

        layout.addLayout(form);

        SettingWidget::lineEdit(
            "مسار Streamlink المخصص", s.streamlinkPath,
            "مسار المجلد اللي فيه Streamlink")
            ->conditionallyEnabledBy(s.streamlinkUseCustomPath)
            ->addTo(layout, form);

        SettingWidget::dropdown("الجودة المفضّلة", s.preferredQuality)
            ->addTo(layout, form);

        SettingWidget::lineEdit("خيارات إضافية", s.streamlinkOpts, "")
            ->addTo(layout, form);
    }

    {
        layout.addTitle("مشغّل بث مخصص");
        layout.addDescription(
            "تقدر تفتح بثوث Twitch مباشرة في أي مشغّل فيديو يدعم Twitch وله URI Scheme خاص.\nمثل: IINA على macOS و Potplayer (مع إضافة) على ويندوز.\n\nإذا ضبطت هذي القيمة، بيظهر لك خيار \"فتح في مشغّل مخصص\" لما تضغط يمين على رأس القناة.");

        SettingWidget::lineEdit("URI Scheme لمشغّل البث المخصص",
                                s.customURIScheme, "custom-player-scheme://")
            ->addTo(layout);
    }

    {
        auto *form = new QFormLayout;
        layout.addTitle("رافع الصور");

        layout.addDescription(
            "تقدر تحدد موقع مخصص لرفع الصور، مثل imgur.com أو s-ul.eu.<br>شوف " +
            formatRichNamedLink("https://chatterino.com/help/image-uploader",
                                "this guide") +
            " for help.");

        SettingWidget::checkbox("تفعيل رافع الصور", s.imageUploaderEnabled)
            ->addTo(layout);

        SettingWidget::checkbox("اطلب تأكيد قبل رفع أي صورة",
                                s.askOnImageUpload)
            ->addTo(layout);

        layout.addLayout(form);

        SettingWidget::lineEdit("رابط الطلب", s.imageUploaderUrl)
            ->addTo(layout, form);

        SettingWidget::lineEdit("حقل النموذج", s.imageUploaderFormField)
            ->addTo(layout, form);

        SettingWidget::lineEdit("ترويسات إضافية", s.imageUploaderHeaders)
            ->addTo(layout, form);

        SettingWidget::lineEdit("رابط الصورة", s.imageUploaderLink)
            ->addTo(layout, form);

        SettingWidget::lineEdit("رابط الحذف", s.imageUploaderDeletionLink)
            ->addTo(layout, form);

        layout.addDescription(
            "صدّر إعدادات رافع الصور الحالية بصيغة JSON عشان تشاركها، أو استورد إعدادات من الحافظة (متوافق مع صيغة ShareX .sxcu).");

        auto *buttonLayout = new QHBoxLayout;

        auto *importButton = new QPushButton("استيراد الإعدادات من الحافظة");
        importButton->setToolTip(
            "استيراد إعدادات رافع الصور من JSON في الحافظة");
        QObject::connect(importButton, &QPushButton::clicked, [this]() {
            importImageUploaderSettings(this);
        });
        buttonLayout->addWidget(importButton);

        auto *exportButton = new QPushButton("تصدير الإعدادات إلى الحافظة");
        exportButton->setToolTip(
            "نسخ إعدادات رافع الصور الحالية إلى الحافظة بصيغة JSON");
        QObject::connect(exportButton, &QPushButton::clicked, [this]() {
            exportImageUploaderSettings(this);
        });
        buttonLayout->addWidget(exportButton);

        buttonLayout->addStretch();
        layout.addLayout(buttonLayout);
    }

#ifdef CHATTERINO_WITH_SPELLCHECK
    {
        // auto *form = new QFormLayout;
        layout.addTitle("المدقق الإملائي (تجريبي)");

        layout.addDescription(
            u"Check the spelling of words in the input box of splits."
            " Chatterino does not include dictionaries - they have to "
            "be downloaded or created manually. Chatterino expects "
            "Hunspell "
            "dictionaries in " %
            formatRichNamedLink(getApp()->getPaths().dictionariesDirectory,
                                getApp()->getPaths().dictionariesDirectory) %
            u". Dictionaries are pairs of .aff (affixes) and .dic (dictionary) "
            u"files.");

        SettingWidget::checkbox("تدقيق إملائي افتراضيًا",
                                s.enableSpellChecking)
            ->setTooltip("دقّق إملاء الكلمات في خانة الكتابة لكل الأقسام افتراضيًا.")
            ->addTo(layout);
        SettingWidget::intInput("عدد الاقتراحات في القائمة",
                                s.nSpellCheckingSuggestions,
                                {
                                    .min = -1,
                                    .max = std::numeric_limits<int>::max(),
                                })
            ->setTooltip(
                "لما تضغط يمين على أي كلمة، يعرض هذا العدد من الاقتراحات. إذا كان 0 ما يعرض شي، وإذا كان -1 بدون حد.")
            ->addTo(layout);

        auto toItem =
            [](const DictionaryInfo &dict) -> std::pair<QString, QVariant> {
            return {
                dict.name,
                dict.path,
            };
        };
        std::vector<std::pair<QString, QVariant>> dictList{{"None", ""}};

        std::ranges::transform(
            getApp()->getSpellChecker()->getAvailableDictionaries(),
            std::back_inserter(dictList), toItem);

        if (dictList.size() > 1)
        {
            SettingWidget::dropdown("القاموس الافتراضي (يتطلب إعادة تشغيل)",
                                    s.spellCheckingDefaultDictionary, dictList)
                ->addTo(layout);
        }
    }
#endif

    layout.addStretch();
}

}  // namespace chatterino
