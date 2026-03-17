// Stub implementation for production API compilation
// This ensures all production API headers are syntactically correct

// Only include full headers if QT_HTTPSERVER is available
#if defined(QT_HTTPSERVER_LIB) || defined(QT_HTTPSERVER_MODULE)

#include "production_api_server.h"
#include "production_api_configuration.h"
#include "rest_api_server.h"
#include "oauth2_manager.h"
#include "jwt_validator.h"
#include "config_manager.h"
#include "pqc_crypto.h"
#include "database_manager.h"
#include "metrics_emitter.h"
#include "ast_analyzer.h"

// Force template instantiation for key classes
namespace RawrXD {
namespace API {
    // Ensure ProductionAPIServer can be instantiated
    template class std::unique_ptr<ProductionAPIServer::Impl>;
}
}

#endif // QT_HTTPSERVER_LIB || QT_HTTPSERVER_MODULE
