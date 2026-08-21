#ifndef CRESCENTNAVIGATORDIALOG_HPP
#define CRESCENTNAVIGATORDIALOG_HPP

#include "StelDialog.hpp"
#include "VisibilityMath.hpp"

class VisibilityContours;
class Ui_CrescentNavigatorDialog;
class QEvent;
class QLabel;
class QPushButton;

class CrescentNavigatorDialog : public StelDialog
{
    Q_OBJECT

public:
    enum class StatusMessage
    {
        Ready,
        EarthOnly,
        Unavailable,
        NotFound
    };

    explicit CrescentNavigatorDialog(VisibilityContours* module);
    ~CrescentNavigatorDialog() override;

    void setStatusMessage(StatusMessage message);
    void setEventStatus(VisibilityMath::CrescentEventKind kind,
                        int dayIndex, const QString& localDate,
                        const QString& localTime,
                        VisibilityMath::EventTimeBasis basis,
                        int hijriYear, int hijriMonth);
    void setNavigationEnabled(bool enabled);

public slots:
    void setVisible(bool visible) override;
    void retranslate() override;

protected:
    void createDialogContent() override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyStatus();
    void configureButtonTooltips();
    void showButtonTooltip(QPushButton* button, const QPoint& buttonPosition);
    void updateEventFilterDirection();
    void updateEventFilterControls(const QString& value);
    QString hijriMonthName(int month) const;

    VisibilityContours* module;
    Ui_CrescentNavigatorDialog* ui;
    QLabel* hoverTooltip;
    StatusMessage pendingMessage;
    bool pendingIsEvent;
    VisibilityMath::CrescentEventKind pendingEventKind;
    VisibilityMath::EventTimeBasis pendingEventBasis;
    int pendingDayIndex;
    QString pendingLocalDate;
    QString pendingLocalTime;
    int pendingHijriYear;
    int pendingHijriMonth;
    bool pendingEnabled;
};

#endif
