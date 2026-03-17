#pragma once

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QVariant>
#include <QVariantList>
#include <QWidget>
#include <QString>
#include <cstdint>
#include <functional>

namespace RawrXD {

#pragma pack(push, 1)
struct QtParam
{
    uint32_t param_type;
    uint64_t param_value;
    uint32_t param_len;
};
#pragma pack(pop)

class QtMasmBridge : public QObject
{
    Q_OBJECT

public:
    enum SignalId : uint32_t
    {
        ChatMessageReceived = 0x1001u,
        FileOpened = 0x1002u,
        TerminalOutput = 0x1003u,
        HotpatchApplied = 0x1004u,
        EditorTextChanged = 0x1005u,
        PaneResized = 0x1006u,
        FailureDetected = 0x1007u,
        CorrectionApplied = 0x1008u
    };

    using SignalHandler = std::function<void(uint32_t, const QVariantList&)>;
    using PropertySetter = std::function<void(const QVariant&)>;
    using WidgetFactory = std::function<QWidget*(QWidget*)>;

    static QtMasmBridge& instance();

    bool initialize();
    bool isInitialized() const;

    bool registerSignalHandler(uint32_t signalId, SignalHandler handler);
    void unregisterSignalHandler(uint32_t signalId);
    bool emitSignal(uint32_t signalId, const QVariantList& params);

    bool bindProperty(const QString& propertyName, PropertySetter setter, const QVariant& initialValue = {});
    bool updateProperty(const QString& propertyName, const QVariant& value);

    void registerWidgetFactory(const QString& typeName, WidgetFactory factory);
    QWidget* createWidget(const QString& typeName, QWidget* parent = nullptr);

signals:
    void masmSignalReceived(uint32_t signalId, const QVariantList& args);
    void propertyUpdated(const QString& propertyName, const QVariant& newValue);

private:
    explicit QtMasmBridge(QObject* parent = nullptr);
    ~QtMasmBridge() override;

    void dispatchSignal(uint32_t signalId, uint32_t paramCount, const QtParam* params);
    void pumpEvents();

    friend void qt_masm_signal_callback(uint32_t signalId, uint32_t paramCount, const QtParam* params);

    bool m_initialized = false;
    bool m_masmInitialized = false;
    QTimer* m_eventTimer = nullptr;
    QMutex m_signalMutex;
    QMutex m_propertyMutex;
    QMutex m_factoryMutex;
    QHash<uint32_t, SignalHandler> m_signalHandlers;

    struct PropertyBinding
    {
        PropertySetter setter;
        QVariant currentValue;
    };

    QHash<QString, PropertyBinding> m_propertyBindings;
    QHash<QString, WidgetFactory> m_widgetFactories;
};
}

extern "C" void qt_masm_signal_callback(uint32_t signalId, uint32_t paramCount, const RawrXD::QtParam* params);
