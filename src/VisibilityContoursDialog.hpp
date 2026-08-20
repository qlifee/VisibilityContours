#ifndef VISIBILITYCONTOURSDIALOG_HPP
#define VISIBILITYCONTOURSDIALOG_HPP

#include "StelDialog.hpp"

class VisibilityContours;
class Ui_VisibilityContoursDialog;

class VisibilityContoursDialog : public StelDialog
{
    Q_OBJECT

public:
    explicit VisibilityContoursDialog(VisibilityContours* module);
    ~VisibilityContoursDialog() override;

public slots:
    void retranslate() override;

protected:
    void createDialogContent() override;

private:
    VisibilityContours* module;
    Ui_VisibilityContoursDialog* ui;
};

#endif
