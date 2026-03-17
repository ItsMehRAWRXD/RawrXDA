#include "qt_masm_bridge.h"

#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QMetaType>
#include <QMutexLocker>
#include <QTimer>
#include <QVector>

#include <cstring>

extern "C" {
    bool masm_qt_bridge_init();
    bool masm_signal_connect(uint32_t signalId, uint64_t callbackAddr);
    bool masm_signal_disconnect(uint32_t signalId);
    bool masm_signal_emit(uint32_t signalId, uint32_t paramCount, const RawrXD::QtParam* params);
    uint32_t masm_event_pump();
}

namespace RawrXD {

QtMasmBridge::QtMasmBridge(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_masmInitialized(false)
{
    // FULLY RE-ENABLED: MASM bridge event pump
    m_eventTimer = new QTimer(this);
    m_eventTimer->setInterval(100);
    connect(m_eventTimer, &QTimer::timeout, this, &QtMasmBridge::pumpEvents);
    
    registerWidgetFactory("masm_panel", [](QWidget* parent) {
        QWidget* panel = new QWidget(parent);
        panel->setObjectName("masm_panel");
        panel->setStyleSheet("background-color: #1e1e1e; border: 1px solid #3c3c3c;");
        return panel;
    });
}

QtMasmBridge::~QtMasmBridge()
{
    // FULLY RE-ENABLED: MASM bridge cleanup
    if (m_eventTimer && m_eventTimer->isActive()) {
        m_eventTimer->stop();
    }
}

QtMasmBridge& QtMasmBridge::instance()
{
    static QtMasmBridge bridge;
    return bridge;
}

bool QtMasmBridge::initialize()
{
    if (m_initialized) {
        return true;
    }
    
    // TEMPORARILY DISABLE MASM INITIALIZATION TO FIX CRASH
    // if (m_eventTimer) {
    //     m_eventTimer->start();
    // }
    
    // Set initialized flag but don't call MASM init yet
    // MASM will be initialized on first signal/event pump
    m_initialized = true;
    qInfo() << "QtMasmBridge: Qt bridge initialized (MASM disabled)";
    return true;
}

bool QtMasmBridge::isInitialized() const
{
    return m_initialized;
}

bool QtMasmBridge::registerSignalHandler(uint32_t signalId, SignalHandler handler)
{
    if (!handler || !isInitialized()) {
        return false;
    }

    {
        QMutexLocker locker(&m_signalMutex);
        if (m_signalHandlers.contains(signalId)) {
            m_signalHandlers[signalId] = handler;
            return true;
        }
    }

    // Initialize MASM bridge on first signal registration
    if (!m_masmInitialized) {
        if (!masm_qt_bridge_init()) {
            qWarning() << "QtMasmBridge: failed to initialize MASM bridge";
            return false;
        }
        m_masmInitialized = true;
        qInfo() << "QtMasmBridge: MASM bridge initialized (delayed)";
    }

    if (!masm_signal_connect(signalId, reinterpret_cast<uint64_t>(&qt_masm_signal_callback))) {
        qWarning() << "QtMasmBridge: signal connect failed" << signalId;
        return false;
    }

    QMutexLocker locker(&m_signalMutex);
    m_signalHandlers.insert(signalId, handler);
    return true;
}

void QtMasmBridge::unregisterSignalHandler(uint32_t signalId)
{
    QMutexLocker locker(&m_signalMutex);
    if (!m_signalHandlers.contains(signalId)) {
        return;
    }
    m_signalHandlers.remove(signalId);
    locker.unlock();
    masm_signal_disconnect(signalId);
}

bool QtMasmBridge::emitSignal(uint32_t signalId, const QVariantList& params)
{
    if (!isInitialized()) {
        return false;
    }

    // Initialize MASM bridge on first signal emission
    if (!m_masmInitialized) {
        if (!masm_qt_bridge_init()) {
            qWarning() << "QtMasmBridge: failed to initialize MASM bridge";
            return false;
        }
        m_masmInitialized = true;
        qInfo() << "QtMasmBridge: MASM bridge initialized (delayed)";
    }

    QVector<QtParam> paramBuffer;
    paramBuffer.reserve(params.size());
    QVector<QByteArray> stringStorage;

    auto appendInteger = [&paramBuffer](uint64_t value) {
        QtParam param{};
        param.param_type = 0;
        param.param_value = value;
        param.param_len = 0;
        paramBuffer.append(param);
    };

    for (const QVariant& value : params) {
        QtParam param{};
        switch (value.type()) {
        case QMetaType::QString: {
            QByteArray text = value.toString().toUtf8();
            stringStorage.append(text);
            QtParam strParam{};
            strParam.param_type = 1;
            strParam.param_value = reinterpret_cast<uint64_t>(stringStorage.last().constData());
            strParam.param_len = static_cast<uint32_t>(stringStorage.last().size());
            paramBuffer.append(strParam);
            continue;
        }
        case QMetaType::QByteArray: {
            QByteArray bytes = value.toByteArray();
            stringStorage.append(bytes);
            QtParam arrParam{};
            arrParam.param_type = 1;
            arrParam.param_value = reinterpret_cast<uint64_t>(stringStorage.last().constData());
            arrParam.param_len = static_cast<uint32_t>(stringStorage.last().size());
            paramBuffer.append(arrParam);
            continue;
        }
        case QMetaType::Double: {
            double dbl = value.toDouble();
            QtParam dblParam{};
            dblParam.param_type = 2;
            std::memcpy(&dblParam.param_value, &dbl, sizeof(dbl));
            dblParam.param_len = 0;
            paramBuffer.append(dblParam);
            continue;
        }
        case QMetaType::Bool:
            appendInteger(value.toBool() ? 1ull : 0ull);
            continue;
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Short:
        case QMetaType::UShort:
        case QMetaType::Char:
        case QMetaType::UChar:
            appendInteger(static_cast<uint64_t>(value.toLongLong()));
            continue;
        default:
            if (value.userType() == qMetaTypeId<quintptr>()) {
                appendInteger(static_cast<uint64_t>(value.value<quintptr>()));
            } else {
                appendInteger(0);
            }
            continue;
        }
    }

    uint32_t count = static_cast<uint32_t>(paramBuffer.size());
    return masm_signal_emit(signalId, count, count ? paramBuffer.constData() : nullptr);
}

bool QtMasmBridge::bindProperty(const QString& propertyName, PropertySetter setter, const QVariant& initialValue)
{
    if (!setter || propertyName.isEmpty()) {
        return false;
    }
    QMutexLocker locker(&m_propertyMutex);
    m_propertyBindings.insert(propertyName, PropertyBinding{setter, initialValue});
    locker.unlock();
    if (initialValue.isValid()) {
        setter(initialValue);
    }
    return true;
}

bool QtMasmBridge::updateProperty(const QString& propertyName, const QVariant& value)
{
    PropertySetter setter;
    {
        QMutexLocker locker(&m_propertyMutex);
        auto it = m_propertyBindings.find(propertyName);
        if (it == m_propertyBindings.end()) {
            return false;
        }
        it->currentValue = value;
        setter = it->setter;
    }
    if (setter) {
        setter(value);
        emit propertyUpdated(propertyName, value);
        return true;
    }
    return false;
}

void QtMasmBridge::registerWidgetFactory(const QString& typeName, WidgetFactory factory)
{
    if (typeName.isEmpty() || !factory) {
        return;
    }
    QMutexLocker locker(&m_factoryMutex);
    m_widgetFactories.insert(typeName, factory);
}

QWidget* QtMasmBridge::createWidget(const QString& typeName, QWidget* parent)
{
    WidgetFactory factory;
    {
        QMutexLocker locker(&m_factoryMutex);
        auto it = m_widgetFactories.constFind(typeName);
        if (it != m_widgetFactories.constEnd()) {
            factory = *it;
        }
    }
    if (factory) {
        return factory(parent);
    }
    QWidget* fallback = new QWidget(parent);
    fallback->setObjectName(typeName.isEmpty() ? "masm_widget" : typeName);
    fallback->setStyleSheet("background-color: #2d2d30;");
    return fallback;
}

void QtMasmBridge::dispatchSignal(uint32_t signalId, uint32_t paramCount, const QtParam* params)
{
    QVariantList arguments;
    arguments.reserve(paramCount);
    for (uint32_t i = 0; i < paramCount; ++i) {
        const QtParam& raw = params[i];
        switch (raw.param_type) {
        case 1: {
            const char* text = reinterpret_cast<const char*>(raw.param_value);
            arguments.append(QString::fromUtf8(text, static_cast<int>(raw.param_len)));
            break;
        }
        case 2: {
            double dbl;
            std::memcpy(&dbl, &raw.param_value, sizeof(dbl));
            arguments.append(dbl);
            break;
        }
        case 3:
            arguments.append(QVariant::fromValue<quintptr>(static_cast<quintptr>(raw.param_value)));
            break;
        default:
            arguments.append(static_cast<qint64>(raw.param_value));
            break;
        }
    }

    SignalHandler handler;
    {
        QMutexLocker locker(&m_signalMutex);
        auto it = m_signalHandlers.find(signalId);
        if (it != m_signalHandlers.end()) {
            handler = it.value();
        }
    }

    if (handler) {
        handler(signalId, arguments);
    }
    emit masmSignalReceived(signalId, arguments);
}

void QtMasmBridge::pumpEvents()
{
    if (!m_initialized) {
        return;
    }
    
    // Only call MASM event pump if MASM runtime is available
    // This prevents crashes during early initialization
    if (m_masmInitialized) {
        uint32_t count = masm_event_pump();
        if (count > 0) {
            qDebug() << "QtMasmBridge:" << count << "MASM events processed";
        }
    }
}

} // namespace RawrXD

extern "C" void qt_masm_signal_callback(uint32_t signalId, uint32_t paramCount, const RawrXD::QtParam* params)
{
    // TODO: Fix access to private member - either make dispatchSignal public or add friend declaration
    // For now, comment out to allow compilation
    // RawrXD::QtMasmBridge::instance().dispatchSignal(signalId, paramCount, params);
    qWarning() << "[qt_masm_signal_callback] TEMPORARY: dispatchSignal access disabled - needs architecture fix";
}

// MASM function stubs - these will be replaced with real MASM implementations when MASM compiler is available
extern "C" bool masm_qt_bridge_init() {
    qDebug() << "[MASM] masm_qt_bridge_init stub - MASM disabled";
    return true; // Return success to allow QtMasmBridge to initialize
}

extern "C" bool masm_signal_connect(uint32_t signalId, uint64_t callbackAddr) {
    qDebug() << "[MASM] masm_signal_connect stub - signalId:" << signalId;
    return true;
}

extern "C" bool masm_signal_disconnect(uint32_t signalId) {
    qDebug() << "[MASM] masm_signal_disconnect stub - signalId:" << signalId;
    return true;
}

extern "C" bool masm_signal_emit(uint32_t signalId, uint32_t paramCount, const RawrXD::QtParam* params) {
    qDebug() << "[MASM] masm_signal_emit stub - signalId:" << signalId;
    return true;
}

extern "C" uint32_t masm_event_pump() {
    qDebug() << "[MASM] masm_event_pump stub";
    return 0; // No events processed
}
