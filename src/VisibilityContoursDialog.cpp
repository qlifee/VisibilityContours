#include "VisibilityContoursDialog.hpp"

#include "Dialog.hpp"
#include "StelApp.hpp"
#include "VisibilityContours.hpp"
#include "ui_VisibilityContoursDialog.h"

#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>

VisibilityContoursDialog::VisibilityContoursDialog(VisibilityContours* contours)
    : StelDialog("VisibilityContours")
    , module(contours)
    , ui(new Ui_VisibilityContoursDialog)
{
}

VisibilityContoursDialog::~VisibilityContoursDialog()
{
    delete ui;
}

void VisibilityContoursDialog::createDialogContent()
{
    ui->setupUi(dialog);
    ui->criterionCombo->addItem(tr("Yallop"), QStringLiteral("Yallop"));
    ui->criterionCombo->addItem(tr("Odeh"), QStringLiteral("Odeh"));

    const int index = ui->criterionCombo->findData(module->criterion());
    ui->criterionCombo->setCurrentIndex(index < 0 ? 0 : index);
    ui->fillBandsCheck->setChecked(module->fillBands());
    ui->navigatorCheck->setChecked(module->navigatorVisible());

    connect(&StelApp::getInstance(), &StelApp::languageChanged,
            this, &VisibilityContoursDialog::retranslate);
    connect(ui->titleBar, &TitleBar::closeClicked,
            this, &StelDialog::close);
    connect(ui->titleBar, &TitleBar::movedTo,
            this, &StelDialog::handleMovedTo);
    connect(ui->criterionCombo, &QComboBox::currentIndexChanged, this,
            [this](int i) { module->setCriterion(ui->criterionCombo->itemData(i).toString()); });
    connect(ui->fillBandsCheck, &QCheckBox::toggled,
            module, &VisibilityContours::setFillBands);
    connect(ui->navigatorCheck, &QCheckBox::toggled,
            module, &VisibilityContours::setNavigatorVisible);
    connect(module, &VisibilityContours::navigatorVisibleChanged,
            ui->navigatorCheck, &QCheckBox::setChecked);
    connect(ui->closeButton, &QPushButton::clicked, this, &StelDialog::close);
}

void VisibilityContoursDialog::retranslate()
{
    if (dialog)
    {
        ui->retranslateUi(dialog);
        const int yallopIndex = ui->criterionCombo->findData(QStringLiteral("Yallop"));
        if (yallopIndex >= 0)
            ui->criterionCombo->setItemText(yallopIndex, tr("Yallop"));
        const int odehIndex = ui->criterionCombo->findData(QStringLiteral("Odeh"));
        if (odehIndex >= 0)
            ui->criterionCombo->setItemText(odehIndex, tr("Odeh"));
    }
}
