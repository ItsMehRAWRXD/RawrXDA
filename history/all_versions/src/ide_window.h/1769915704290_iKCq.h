#include "ide_window.h"
#include "../autonomous_model_manager.h"
#include "../intelligent_codebase_engine.h"
#include "../autonomous_feature_engine.h"
#include "../ai_integration_hub.h"

// ...existing code...

namespace RawrXD { class EditorWindow; }

// ...existing code...

    HWND hReplaceDialog_;
    RawrXD::EditorWindow* pEditor; 
    
    std::shared_ptr<AutonomousModelManager> modelManager;
    std::shared_ptr<IntelligentCodebaseEngine> codebaseEngine;
    std::shared_ptr<AutonomousFeatureEngine> featureEngine;
    std::shared_ptr<RawrXD::AIIntegrationHub> aiHub;

    // Web Browser Interface
// ...existing code...