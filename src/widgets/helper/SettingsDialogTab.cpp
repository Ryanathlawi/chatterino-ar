// SPDX-FileCopyrightText: 2017 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/SettingsDialogTab.hpp"

#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/settingspages/SettingsPage.hpp"

#include <QPainter>
#include <QStyleOption>

namespace chatterino {

SettingsDialogTab::SettingsDialogTab(SettingsDialog *_dialog,
                                     std::function<SettingsPage *()> _lazyPage,
                                     const QString &name, QString imageFileName,
                                     SettingsTabId id)
    : BaseWidget(_dialog)
    , dialog_(_dialog)
    , lazyPage_(std::move(_lazyPage))
    , id_(id)
    , name_(name)
{
    this->ui_.labelText = name;
    this->ui_.icon.addFile(imageFileName);

    this->setCursor(QCursor(Qt::PointingHandCursor));

    this->setStyleSheet("color: #FFF");
}

void SettingsDialogTab::setSelected(bool _selected)
{
    if (this->selected_ == _selected)
    {
        return;
    }

    //    height: <checkbox-size>px;

    this->selected_ = _selected;
    this->selectedChanged(this->selected_);
}

SettingsPage *SettingsDialogTab::page()
{
    if (this->page_)
    {
        return this->page_;
    }

    this->page_ = this->lazyPage_();
    this->page_->setTab(this);
    return this->page_;
}

void SettingsDialogTab::paintEvent(QPaintEvent *)
{
    // Arabic build: RTL-aware pill item: [icon][text] from the start side
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    float scale = this->scale();
    bool rtl = this->layoutDirection() == Qt::RightToLeft;

    QRectF pill = QRectF(this->rect()).adjusted(6 * scale, 2 * scale,
                                                -6 * scale, -2 * scale);
    if (this->selected_ || this->underMouse())
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(this->selected_ ? QColor("#1f1f26")
                                         : QColor("#18181d"));
        painter.drawRoundedRect(pill, 6 * scale, 6 * scale);
    }
    if (this->selected_)
    {
        qreal w = 3 * scale;
        qreal x = rtl ? pill.right() - w : pill.left();
        painter.setBrush(QColor("#1fae6a"));
        painter.drawRoundedRect(
            QRectF(x, pill.top() + 8 * scale, w, pill.height() - 16 * scale),
            w / 2, w / 2);
    }

    int iconSize = int(18 * scale);
    int pad = int(16 * scale);
    int iy = (this->height() - iconSize) / 2;
    int ix = rtl ? this->width() - pad - iconSize : pad;
    painter.drawPixmap(ix, iy, this->ui_.icon.pixmap(QSize(iconSize, iconSize)));

    int gap = int(12 * scale);
    QRect textRect = rtl ? QRect(pad, 0, ix - gap - pad, this->height())
                         : QRect(ix + iconSize + gap, 0,
                                 this->width() - ix - iconSize - gap - pad,
                                 this->height());
    QFont f = this->font();
    f.setBold(this->selected_);
    painter.setFont(f);
    painter.setPen(this->selected_ ? QColor("#ffffff") : QColor("#9a9aa4"));
    painter.drawText(textRect,
                     (rtl ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter,
                     this->ui_.labelText);
}

void SettingsDialogTab::enterEvent(QEnterEvent *)
{
    this->update();
}

void SettingsDialogTab::leaveEvent(QEvent *)
{
    this->update();
}

void SettingsDialogTab::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        return;
    }

    this->dialog_->selectTab(this);

    this->setFocus();
}

const QString &SettingsDialogTab::name() const
{
    return this->name_;
}

SettingsTabId SettingsDialogTab::id() const
{
    return this->id_;
}

}  // namespace chatterino
