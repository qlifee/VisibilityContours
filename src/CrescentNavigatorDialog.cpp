#include "CrescentNavigatorDialog.hpp"

#include "Dialog.hpp"
#include "StelApp.hpp"
#include "StelFileMgr.hpp"
#include "StelLocaleMgr.hpp"
#include "StelMainView.hpp"
#include "VisibilityContours.hpp"
#include "ui_CrescentNavigatorDialog.h"

#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>

CrescentNavigatorDialog::CrescentNavigatorDialog(VisibilityContours* contours)
    : StelDialog("VisibilityContoursNavigator")
    , module(contours)
    , ui(new Ui_CrescentNavigatorDialog)
    , pendingMessage(StatusMessage::Ready)
    , pendingIsEvent(false)
    , pendingEventKind(VisibilityMath::CrescentEventKind::Morning)
    , pendingEventBasis(VisibilityMath::EventTimeBasis::BestTime)
    , pendingDayIndex(0)
    , pendingHijriYear(0)
    , pendingHijriMonth(0)
    , pendingEnabled(true)
{
    connect(this, &StelDialog::visibleChanged, this, [this](bool visible)
    {
        if (!visible && module && module->navigatorVisible())
            module->setNavigatorVisible(false);
    });
}

CrescentNavigatorDialog::~CrescentNavigatorDialog()
{
    delete ui;
}

void CrescentNavigatorDialog::createDialogContent()
{
    ui->setupUi(dialog);
    updateEventFilterDirection();
    applyStatus();
    ui->backButton->setEnabled(pendingEnabled);
    ui->forwardButton->setEnabled(pendingEnabled);
    ui->allBackButton->setEnabled(pendingEnabled);
    ui->allForwardButton->setEnabled(pendingEnabled);
    updateEventFilterControls(module->eventFilter());

    connect(&StelApp::getInstance(), &StelApp::languageChanged,
            this, &CrescentNavigatorDialog::retranslate);
    connect(ui->titleBar, &TitleBar::closeClicked,
            this, &StelDialog::close);
    connect(ui->titleBar, &TitleBar::movedTo,
            this, &StelDialog::handleMovedTo);
    connect(ui->backButton, &QPushButton::clicked,
            module, &VisibilityContours::navigateBackward);
    connect(ui->forwardButton, &QPushButton::clicked,
            module, &VisibilityContours::navigateForward);
    connect(ui->allBackButton, &QPushButton::clicked,
            module, &VisibilityContours::navigateAllBackward);
    connect(ui->allForwardButton, &QPushButton::clicked,
            module, &VisibilityContours::navigateAllForward);
    connect(ui->bothRadio, &QRadioButton::clicked, this,
            [this]() { module->setEventFilter(QStringLiteral("Both")); });
    connect(ui->morningRadio, &QRadioButton::clicked, this,
            [this]() { module->setEventFilter(QStringLiteral("Morning")); });
    connect(ui->eveningRadio, &QRadioButton::clicked, this,
            [this]() { module->setEventFilter(QStringLiteral("Evening")); });
    connect(module, &VisibilityContours::eventFilterChanged, this,
            &CrescentNavigatorDialog::updateEventFilterControls);
}

void CrescentNavigatorDialog::setVisible(bool visible)
{
    bool useInitialPosition = false;
    if (visible && !dialog)
    {
        // Avoid StelApp::getSettings(): that accessor is inline and therefore
        // unsafe when a distro Stellarium binary has a different private class
        // layout from the SDK reference tree used to compile this plugin.
        QSettings positionSettings(
            StelFileMgr::getUserDir() + QStringLiteral("/config.ini"),
            QSettings::IniFormat);
        useInitialPosition = !positionSettings.contains(
            QStringLiteral("DialogPositions/") + getDialogName());
    }

    StelDialog::setVisible(visible);

    if (visible && proxy)
    {
        constexpr int margin = 20;
        const QSize screenSize = StelMainView::getInstance().size();
        const int maximumX = qMax(
            margin,
            screenSize.width() - static_cast<int>(proxy->size().width()) - margin);
        const int maximumY = qMax(
            margin,
            screenSize.height() - static_cast<int>(proxy->size().height()) - margin);
        QPoint position = proxy->pos().toPoint();
        if (useInitialPosition)
            position = QPoint(maximumX, margin);
        else
        {
            position.setX(qBound(margin, position.x(), maximumX));
            position.setY(qBound(margin, position.y(), maximumY));
        }
        if (proxy->pos().toPoint() != position)
        {
            proxy->setPos(position);
            handleMovedTo(position);
        }
    }
}

void CrescentNavigatorDialog::setStatusMessage(StatusMessage message)
{
    pendingMessage = message;
    pendingIsEvent = false;
    applyStatus();
}

void CrescentNavigatorDialog::setEventStatus(
    VisibilityMath::CrescentEventKind kind, int dayIndex,
    const QString& localDate, const QString& localTime,
    VisibilityMath::EventTimeBasis basis, int hijriYear, int hijriMonth)
{
    pendingIsEvent = true;
    pendingEventKind = kind;
    pendingEventBasis = basis;
    pendingDayIndex = dayIndex;
    pendingLocalDate = localDate;
    pendingLocalTime = localTime;
    pendingHijriYear = hijriYear;
    pendingHijriMonth = hijriMonth;
    applyStatus();
}

void CrescentNavigatorDialog::setNavigationEnabled(bool enabled)
{
    pendingEnabled = enabled;
    if (dialog)
    {
        ui->backButton->setEnabled(enabled);
        ui->forwardButton->setEnabled(enabled);
        ui->allBackButton->setEnabled(enabled);
        ui->allForwardButton->setEnabled(enabled);
    }
}

void CrescentNavigatorDialog::retranslate()
{
    if (dialog)
    {
        ui->retranslateUi(dialog);
        updateEventFilterDirection();
        applyStatus();
    }
}

void CrescentNavigatorDialog::updateEventFilterDirection()
{
    if (!dialog)
        return;
    const QString language =
        StelApp::getInstance().getLocaleMgr().getAppLanguage();
    const bool useRightToLeft = VisibilityMath::useArabicForProgramLanguage(
        language.toStdString());
    ui->eventFilterLayout->setDirection(
        useRightToLeft ? QBoxLayout::RightToLeft : QBoxLayout::LeftToRight);
    const Qt::LayoutDirection widgetDirection =
        useRightToLeft ? Qt::RightToLeft : Qt::LeftToRight;
    ui->navigateLabel->setLayoutDirection(widgetDirection);
    ui->bothRadio->setLayoutDirection(widgetDirection);
    ui->morningRadio->setLayoutDirection(widgetDirection);
    ui->eveningRadio->setLayoutDirection(widgetDirection);
}

void CrescentNavigatorDialog::updateEventFilterControls(const QString& value)
{
    if (!dialog)
        return;
    switch (VisibilityMath::eventFilterFromString(value.toStdString()))
    {
    case VisibilityMath::EventFilter::Morning:
        ui->morningRadio->setChecked(true);
        break;
    case VisibilityMath::EventFilter::Evening:
        ui->eveningRadio->setChecked(true);
        break;
    case VisibilityMath::EventFilter::Both:
    default:
        ui->bothRadio->setChecked(true);
        break;
    }
}

QString CrescentNavigatorDialog::hijriMonthName(int month) const
{
    switch (month)
    {
    case 1:  return tr("Muharram");
    case 2:  return tr("Safar");
    case 3:  return tr("Rabi' al-Awwal");
    case 4:  return tr("Rabi' al-Akhir");
    case 5:  return tr("Jumada al-Ula");
    case 6:  return tr("Jumada al-Akhirah");
    case 7:  return tr("Rajab");
    case 8:  return tr("Sha'ban");
    case 9:  return tr("Ramadan");
    case 10: return tr("Shawwal");
    case 11: return tr("Dhu al-Qi'dah");
    case 12: return tr("Dhu al-Hijjah");
    default: return {};
    }
}

void CrescentNavigatorDialog::applyStatus()
{
    if (!dialog)
        return;
    if (pendingIsEvent)
    {
        const QString kind = pendingEventKind == VisibilityMath::CrescentEventKind::Morning
                                 ? tr("Morning") : tr("Evening");
        QString basis;
        switch (pendingEventBasis)
        {
        case VisibilityMath::EventTimeBasis::Sunrise:
            basis = tr("Sunrise");
            break;
        case VisibilityMath::EventTimeBasis::Sunset:
            basis = tr("Sunset");
            break;
        case VisibilityMath::EventTimeBasis::BestTime:
        default:
            basis = tr("Best time");
            break;
        }
        const QString dayText = pendingDayIndex >= 0
                                    ? QStringLiteral("+%1").arg(pendingDayIndex)
                                    : QString::number(pendingDayIndex);
        ui->statusLabel->setText(
            tr("%1 · day %2 · %3").arg(kind, dayText, basis));
        ui->timeLabel->setText(
            QStringLiteral("%1 · %2").arg(pendingLocalDate, pendingLocalTime));
        const QString monthName = hijriMonthName(pendingHijriMonth);
        const bool validHijri = !monthName.isEmpty() && pendingHijriYear != 0;
        ui->hijriLabel->setVisible(validHijri);
        if (validHijri)
        {
            ui->hijriLabel->setText(
                pendingEventKind == VisibilityMath::CrescentEventKind::Morning
                    ? tr("End of %1 %2 AH").arg(monthName).arg(pendingHijriYear)
                    : tr("Beginning of %1 %2 AH").arg(monthName).arg(pendingHijriYear));
        }
    }
    else
    {
        QString status;
        switch (pendingMessage)
        {
        case StatusMessage::EarthOnly:
            status = tr("Moon navigation is available only on Earth");
            break;
        case StatusMessage::Unavailable:
            status = tr("Moon navigation is not available");
            break;
        case StatusMessage::NotFound:
            status = tr("No valid Moon event found");
            break;
        case StatusMessage::Ready:
        default:
            status = tr("Ready");
            break;
        }
        ui->statusLabel->setText(status);
        ui->timeLabel->clear();
        ui->hijriLabel->clear();
        ui->hijriLabel->setVisible(false);
    }
    dialog->adjustSize();
}
