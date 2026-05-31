/**
 * @file quickjs_extension_host.h
 * @brief QuickJS Extension Host — Wires QuickJS to RawrXD Extension System
 *
 * Loads JS/TS extensions via QuickJS runtime, exposes VS Code API bridge
 * to JS context, and manages activation/deactivation lifecycle.
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace RawrXD::Extensions {

class VSCodeAPIBridge;
class ProcessBroker;
class OSSandbox;

/**
 * @brief QuickJS-based extension host for JS/TS extensions.
 *
 * Provides an isolated QuickJS runtime per extension with VS Code API
 * compatibility, memory limits, and lifecycle management.
 */
class QuickJSExtensionHost {
public:
    QuickJSExtensionHost();
    ~QuickJSExtensionHost();

    // Non-copyable, non-movable
    QuickJSExtensionHost(const QuickJSExtensionHost&) = delete;
    QuickJSExtensionHost& operator=(const QuickJSExtensionHost&) = delete;

    /**
     * @brief Initialize the host with required dependencies.
     * @param apiBridge VS Code API bridge for JS context injection.
     * @param broker Process broker for out-of-process isolation (optional).
     * @param sandbox OS sandbox for security enforcement (optional).
     * @return true on success.
     */
    bool initialize(VSCodeAPIBridge* apiBridge,
                    ProcessBroker* broker = nullptr,
                    OSSandbox* sandbox = nullptr);

    /** @brief Shutdown all runtimes and free resources. */
    void shutdown();

    /**
     * @brief Load an extension from disk into a new QuickJS runtime.
     * @param extId Unique extension identifier.
     * @param path Directory containing extension.js (or out/extension.js, dist/extension.js).
     * @return true if loaded successfully.
     */
    bool loadExtension(const std::string& extId, const std::string& path);

    /**
     * @brief Activate a loaded extension (calls activate() in JS).
     * @param extId Extension identifier.
     * @return true if activated.
     */
    bool activateExtension(const std::string& extId);

    /**
     * @brief Deactivate an active extension (calls deactivate() in JS).
     * @param extId Extension identifier.
     * @return true if deactivated.
     */
    bool deactivateExtension(const std::string& extId);

    /**
     * @brief Unload an extension and destroy its runtime.
     * @param extId Extension identifier.
     * @return true if unloaded.
     */
    bool unloadExtension(const std::string& extId);

    /** @brief Check if extension is loaded. */
    bool isExtensionLoaded(const std::string& extId) const;

    /** @brief Check if extension is active. */
    bool isExtensionActive(const std::string& extId) const;

    /** @brief Number of loaded extensions. */
    size_t getLoadedCount() const;

    /** @brief Number of active extensions. */
    size_t getActiveCount() const;

    /** @brief List all loaded extension IDs. */
    std::vector<std::string> listLoaded() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace RawrXD::Extensions
