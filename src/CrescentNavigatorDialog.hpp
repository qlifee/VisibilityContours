#ifndef CRESCENTNAVIGATORDIALOG_HPP
#define CRESCENTNAVIGATORDIALOG_HPP

#include "StelDialog.hpp"

class VisibilityContours;
class Ui_CrescentNavigatorDialog;

class CrescentNavigatorDialog : public StelDialog
{
    Q_OBJECT

public:
    explicit CrescentNavigatorDialog(VisibilityContours* module);
    ~CrescentNavigatorDialog() override;

    void setStatus(const QString& primaryText,
                   const QString& secondaryText = QString());
    void setNavigationEnabled(bool enabled);

public slots:
    void setVisible(bool visible) override;
    void retranslate() override;

protected:
    void createDialogContent() override;

private:
    void applyStatus();

    VisibilityContours* module;
    Ui_CrescentNavigatorDialog* ui;
    QString pendingPrimaryStatus;
    QString pendingSecondaryStatus;
    bool pendingEnabled;
};

#endif
