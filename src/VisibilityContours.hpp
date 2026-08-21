#ifndef VISIBILITYCONTOURS_HPP
#define VISIBILITYCONTOURS_HPP

#include "StelModule.hpp"
#include "StelPluginInterface.hpp"
#include "VisibilityMath.hpp"

#include <QObject>

class QSettings;
class QTranslator;
class CrescentNavigatorDialog;
class VisibilityContoursDialog;

class VisibilityContours : public StelModule
{
    Q_OBJECT
    Q_PROPERTY(QString criterion READ criterion WRITE setCriterion NOTIFY criterionChanged)
    Q_PROPERTY(bool fillBands READ fillBands WRITE setFillBands NOTIFY fillBandsChanged)
    Q_PROPERTY(bool navigatorVisible READ navigatorVisible WRITE setNavigatorVisible
               NOTIFY navigatorVisibleChanged)

public:
    VisibilityContours();
    ~VisibilityContours() override;

    void init() override;
    void draw(StelCore* core) override;
    double getCallOrder(StelModuleActionName actionName) const override;
    bool configureGui(bool show = true) override;

    QString criterion() const;
    bool fillBands() const;
    bool navigatorVisible() const;

public slots:
    void setCriterion(const QString& value);
    void setFillBands(bool enabled);
    void setNavigatorVisible(bool visible);
    void navigateForward();
    void navigateBackward();
    void navigateAllForward();
    void navigateAllBackward();

signals:
    void criterionChanged(const QString& value);
    void fillBandsChanged(bool enabled);
    void navigatorVisibleChanged(bool visible);

private:
    void readSettings();
    void saveSettings() const;
    void addMoonInformation(StelCore* core);
    void navigateToCrescent(int direction, VisibilityMath::NavigationMode mode);
    void updateNavigatorAvailability(StelCore* core);

    double cachedForJDE;
    double cachedConjunctionJDE;
    double cachedBestLocalDay;
    double cachedBestLatitude;
    double cachedBestLongitude;
    int cachedBestAltitude;
    double cachedEveningJd;
    double cachedEveningV;
    double cachedMorningJd;
    double cachedMorningV;
    bool cachedEveningAvailable;
    bool cachedMorningAvailable;
    double cachedBestJd;
    double cachedBestV;
    bool cachedBestAvailable;
    QString selectedCriterion;
    bool bandsFilled;
    bool navigatorShown;
    bool navigatorEarthAvailable;
    QSettings* settings;
    VisibilityContoursDialog* configDialog;
    CrescentNavigatorDialog* navigatorDialog;
};

class VisibilityContoursStelPluginInterface : public QObject, public StelPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID StelPluginInterface_iid)
    Q_INTERFACES(StelPluginInterface)

public:
    VisibilityContoursStelPluginInterface();
    ~VisibilityContoursStelPluginInterface() override;

    StelModule* getStelModule() const override;
    StelPluginInfo getPluginInfo() const override;

private slots:
    void refreshTranslation();

private:
    QTranslator* arabicTranslator;
    bool translatorInstalled;
};

#endif // VISIBILITYCONTOURS_HPP
