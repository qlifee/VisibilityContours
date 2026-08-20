#include "VisibilityContoursDialog.hpp"

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
    ui->criterionCombo->addItem(tr("Odeh"), QStringLiteral("Odeh"));
    ui->criterionCombo->addItem(tr("Yallop"), QStringLiteral("Yallop"));

    const int index = ui->criterionCombo->findData(module->criterion());
    ui->criterionCombo->setCurrentIndex(index < 0 ? 0 : index);
    ui->fillBandsCheck->setChecked(module->fillBands());

    connect(ui->criterionCombo, &QComboBox::currentIndexChanged, this,
            [this](int i) { module->setCriterion(ui->criterionCombo->itemData(i).toString()); });
    connect(ui->fillBandsCheck, &QCheckBox::toggled,
            module, &VisibilityContours::setFillBands);
    connect(ui->closeButton, &QPushButton::clicked, this, &StelDialog::close);
}

void VisibilityContoursDialog::retranslate()
{
    if (dialog)
        ui->retranslateUi(dialog);
}
