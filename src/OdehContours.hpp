#ifndef ODEHCONTOURS_HPP
#define ODEHCONTOURS_HPP

#include "StelModule.hpp"
#include "StelPluginInterface.hpp"

#include <QObject>

class OdehContours : public StelModule
{
public:
    OdehContours();
    ~OdehContours() override;

    void init() override;
    void draw(StelCore* core) override;
    double getCallOrder(StelModuleActionName actionName) const override;

private:
    double cachedForJDE;
    double cachedConjunctionJDE;
};

class OdehContoursStelPluginInterface : public QObject, public StelPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID StelPluginInterface_iid)
    Q_INTERFACES(StelPluginInterface)

public:
    StelModule* getStelModule() const override;
    StelPluginInfo getPluginInfo() const override;
};

#endif // ODEHCONTOURS_HPP
