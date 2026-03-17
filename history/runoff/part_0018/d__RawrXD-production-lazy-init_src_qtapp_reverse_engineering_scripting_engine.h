/**
 * \file scripting_engine.h
 * \brief Scripting engine for custom reverse engineering analysis
 * \author RawrXD Team
 * \date 2026-01-22
 */

#pragma once

#include <QString>
#include <QMap>
#include <QVector>
#include <functional>

namespace RawrXD {
namespace ReverseEngineering {

/**
 * \class ScriptingEngine
 * \brief Execute custom analysis scripts (Python/Lua/JavaScript)
 */
class ScriptingEngine {
public:
    /**
     * \brief Construct scripting engine
     */
    explicit ScriptingEngine();
    
    /**
     * \brief Destructor - cleanup scripting context
     */
    ~ScriptingEngine();
    
    /**
     * \brief Execute Python script
     * \param script Python code to execute
     * \param context Analysis context (binary, functions, etc.)
     * \return Script output/result
     */
    QString executePython(const QString& script, const QMap<QString, QVariant>& context);
    
    /**
     * \brief Execute Lua script
     * \param script Lua code to execute
     * \param context Analysis context
     * \return Script output/result
     */
    QString executeLua(const QString& script, const QMap<QString, QVariant>& context);
    
    /**
     * \brief Execute JavaScript script
     * \param script JavaScript code to execute
     * \param context Analysis context
     * \return Script output/result
     */
    QString executeJavaScript(const QString& script, const QMap<QString, QVariant>& context);
    
    /**
     * \brief Register custom function for scripts
     * \param name Function name
     * \param func Callable function
     */
    void registerFunction(const QString& name, std::function<QString(const QVector<QString>&)> func);
    
    /**
     * \brief Get last error message
     * \return Error string
     */
    QString lastError() const { return m_lastError; }
    
    /**
     * \brief Load script file
     * \param filePath Path to script
     * \return Script contents
     */
    static QString loadScript(const QString& filePath);
    
    /**
     * \brief Save script file
     * \param filePath Path to save
     * \param script Script contents
     * \return True if successful
     */
    static bool saveScript(const QString& filePath, const QString& script);

private:
    QString m_lastError;
    QMap<QString, std::function<QString(const QVector<QString>&)>> m_functions;
    
    void initializeContextVariables(const QMap<QString, QVariant>& context);
    QString formatOutput(const QString& raw);
};

} // namespace ReverseEngineering
} // namespace RawrXD
