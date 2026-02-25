#pragma once

/**
 * @file marketplace.h
 * @brief Main header for the VS Code Extension Marketplace implementation
 * 
 * This header provides easy access to all marketplace components:
 * - ExtensionMarketplaceManager: Core marketplace functionality
 * - VsixInstaller: VSIX package handling
 * - EnterprisePolicyEngine: Enterprise security and compliance
 * - OfflineCacheStore: Offline caching and air-gapped deployment
 * - MarketplaceUIView: UI components for the marketplace
 */

// Core marketplace components
#include "marketplace/extension_marketplace_manager.h"
#include "marketplace/vsix_installer.h"
#include "marketplace/enterprise_policy_engine.h"
#include "marketplace/offline_cache_store.h"
#include "marketplace/marketplace_ui_view.h"