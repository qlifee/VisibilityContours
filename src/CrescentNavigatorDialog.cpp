#include "CrescentNavigatorDialog.hpp"

#include "Dialog.hpp"
#include "StelApp.hpp"
#include "StelFileMgr.hpp"
#include "StelLocaleMgr.hpp"
#include "StelMainView.hpp"
#include "VisibilityContours.hpp"
#include "ui_CrescentNavigatorDialog.h"

#include <QBoxLayout>
#include <QColor>
#include <QEnterEvent>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QToolTip>

CrescentNavigatorDialog::CrescentNavigatorDialog(VisibilityContours* contours)
    : StelDialog("VisibilityContoursNavigator")
    , module(contours)
    , ui(new Ui_CrescentNavigatorDialog)
    , hoverTooltip(nullptr)
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
    hoverTooltip = new QLabel(dialog);
    hoverTooltip->setObjectName(QStringLiteral("visibilityContoursHoverTooltip"));
    hoverTooltip->setAttribute(Qt::WA_TransparentForMouseEvents);
    const QPalette tooltipPalette = QToolTip::palette();
    QColor background = tooltipPalette.color(QPalette::ToolTipBase);
    QColor foreground = tooltipPalette.color(QPalette::ToolTipText);
    QColor border = tooltipPalette.color(QPalette::Dark);
    background.setAlpha(255);
    foreground.setAlpha(255);
    border.setAlpha(255);
    const auto cssColor = [](const QColor& color)
    {
        return QStringLiteral("rgba(%1, %2, %3, %4)")
            .arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
    };
    hoverTooltip->setStyleSheet(
        QStringLiteral(
            "QLabel#visibilityContoursHoverTooltip {"
            " background-color: %1; color: %2; border: 1px solid %3;"
            " border-radius: 3px; padding: 4px 7px; }"
        ).arg(cssColor(background), cssColor(foreground), cssColor(border)));
    hoverTooltip->setFont(QToolTip::font());
    hoverTooltip->hide();

    updateEventFilterDirection();
    applyStatus();
    ui->backButton->setEnabled(pendingEnabled);
    ui->forwardButton->setEnabled(pendingEnabled);
    ui->allBackButton->setEnabled(pendingEnabled);
    ui->allForwardButton->setEnabled(pendingEnabled);
    updateEventFilterControls(module->eventFilter());

    // StelDialog widgets are embedded in a QGraphicsProxyWidget. Qt's native
    // tooltip window can therefore interpret scene coordinates as screen
    // coordinates. Use a child overlay driven by local mouse coordinates.
    configureButtonTooltips();
    ui->backButton->installEventFilter(this);
    ui->forwardButton->installEventFilter(this);
    ui->allBackButton->installEventFilter(this);
    ui->allForwardButton->installEventFilter(this);

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

bool CrescentNavigatorDialog::eventFilter(QObject* watched, QEvent* event)
{
    auto* button = qobject_cast<QPushButton*>(watched);
    if (button && event->type() == QEvent::Enter)
    {
        const auto* enterEvent = static_cast<QEnterEvent*>(event);
        showButtonTooltip(button, enterEvent->position().toPoint());
    }
    else if (button && event->type() == QEvent::MouseMove)
    {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        showButtonTooltip(button, mouseEvent->position().toPoint());
    }
    else if (button && event->type() == QEvent::Leave && hoverTooltip)
        hoverTooltip->hide();
    return StelDialog::eventFilter(watched, event);
}

void CrescentNavigatorDialog::configureButtonTooltips()
{
    const auto configure = [](QPushButton* button, const QString& text)
    {
        button->setProperty("visibilityContoursTooltip", text);
        button->setToolTip(QString());
        button->setMouseTracking(true);
    };
    configure(ui->backButton, tr("Previous Moon best time"));
    configure(ui->forwardButton, tr("Next Moon best time"));
    configure(ui->allBackButton, tr("Previous Moon event"));
    configure(ui->allForwardButton, tr("Next Moon event"));
}

void CrescentNavigatorDialog::showButtonTooltip(
    QPushButton* button, const QPoint& buttonPosition)
{
    if (!hoverTooltip || !dialog)
        return;
    const QString text = button->property("visibilityContoursTooltip").toString();
    if (text.isEmpty())
        return;

    hoverTooltip->setText(text);
    hoverTooltip->adjustSize();
    const QPoint cursorInDialog = button->mapTo(dialog, buttonPosition);
    const int margin = 4;
    const int maximumX = qMax(margin,
                              dialog->width() - hoverTooltip->width() - margin);
    const int maximumY = qMax(margin,
                              dialog->height() - hoverTooltip->height() - margin);
    const QPoint position(
        qBound(margin, cursorInDialog.x() + 12, maximumX),
        qBound(margin, cursorInDialog.y() + 18, maximumY));
    hoverTooltip->move(position);
    hoverTooltip->raise();
    hoverTooltip->show();
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
        configureButtonTooltips();
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
