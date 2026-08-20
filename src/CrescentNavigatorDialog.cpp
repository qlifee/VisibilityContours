#include "CrescentNavigatorDialog.hpp"

#include "Dialog.hpp"
#include "StelFileMgr.hpp"
#include "StelMainView.hpp"
#include "VisibilityContours.hpp"
#include "ui_CrescentNavigatorDialog.h"

#include <QLabel>
#include <QPushButton>
#include <QSettings>

CrescentNavigatorDialog::CrescentNavigatorDialog(VisibilityContours* contours)
    : StelDialog("VisibilityContoursNavigator")
    , module(contours)
    , ui(new Ui_CrescentNavigatorDialog)
    , pendingPrimaryStatus(tr("Ready"))
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
    applyStatus();
    ui->backButton->setEnabled(pendingEnabled);
    ui->forwardButton->setEnabled(pendingEnabled);

    connect(ui->titleBar, &TitleBar::closeClicked,
            this, &StelDialog::close);
    connect(ui->titleBar, &TitleBar::movedTo,
            this, &StelDialog::handleMovedTo);
    connect(ui->backButton, &QPushButton::clicked,
            module, &VisibilityContours::navigateBackward);
    connect(ui->forwardButton, &QPushButton::clicked,
            module, &VisibilityContours::navigateForward);
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

void CrescentNavigatorDialog::setStatus(const QString& primaryText,
                                        const QString& secondaryText)
{
    pendingPrimaryStatus = primaryText;
    pendingSecondaryStatus = secondaryText;
    applyStatus();
}

void CrescentNavigatorDialog::setNavigationEnabled(bool enabled)
{
    pendingEnabled = enabled;
    if (dialog)
    {
        ui->backButton->setEnabled(enabled);
        ui->forwardButton->setEnabled(enabled);
    }
}

void CrescentNavigatorDialog::retranslate()
{
    if (dialog)
    {
        ui->retranslateUi(dialog);
        applyStatus();
    }
}

void CrescentNavigatorDialog::applyStatus()
{
    if (!dialog)
        return;
    ui->statusLabel->setText(pendingPrimaryStatus);
    ui->timeLabel->setText(pendingSecondaryStatus);
    dialog->adjustSize();
}
