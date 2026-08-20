#ifndef VISIBILITYCONTOURS_HPP
#define VISIBILITYCONTOURS_HPP

#include "StelModule.hpp"
#include "StelPluginInterface.hpp"

#include <QObject>

class QSettings;
class VisibilityContoursDialog;

class VisibilityContours : public StelModule
{
    Q_OBJECT
    Q_PROPERTY(QString criterion READ criterion WRITE setCriterion NOTIFY criterionChanged)
    Q_PROPERTY(bool fillBands READ fillBands WRITE setFillBands NOTIFY fillBandsChanged)

public:
    VisibilityContours();
    ~VisibilityContours() override;

    void init() override;
    void draw(StelCore* core) override;
    double getCallOrder(StelModuleActionName actionName) const override;
    bool configureGui(bool show = true) override;

    QString criterion() const;
    bool fillBands() const;

public slots:
    void setCriterion(const QString& value);
    void setFillBands(bool enabled);

signals:
    void criterionChanged(const QString& value);
    void fillBandsChanged(bool enabled);

private:
    void readSettings();
    void saveSettings() const;
    void addMoonInformation(StelCore* core);

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
    QSettings* settings;
    VisibilityContoursDialog* configDialog;
};

class VisibilityContoursStelPluginInterface : public QObject, public StelPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID StelPluginInterface_iid)
    Q_INTERFACES(StelPluginInterface)

public:
    StelModule* getStelModule() const override;
    StelPluginInfo getPluginInfo() const override;
};

#endif // VISIBILITYCONTOURS_HPP
