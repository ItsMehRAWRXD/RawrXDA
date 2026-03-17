#include "enterprise_production_framework.h"
#include <QDebug>

ProductionFramework::ProductionFramework(QObject* parent)
    : QObject(parent) {
    qDebug() << "[ProductionFramework] Initialized";
}

ProductionFramework::~ProductionFramework() {
    qDebug() << "[ProductionFramework] Destroyed";
}
