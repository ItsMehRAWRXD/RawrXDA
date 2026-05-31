// model_router_cli_test.cpp - Comprehensive CLI Test for Model Router
#include "model_interface.h"
#include "universal_model_router.h"
#include "cloud_api_client.h"


#include <iostream>
#include <memory>

class ModelRouterTester : public void {

public:
    ModelRouterTester(void* parent = nullptr)
        : void(parent),
          test_count(0),
          success_count(0),
          failure_count(0)
    {
        model_interface = std::make_unique<ModelInterface>(this);
// Qt connect removed
// Qt connect removed
// Qt connect removed
    }

    void runTests() {


        // Initialize with config
        std::string config_path = "./model_config.json";
        
        model_interface->initialize(config_path);
    }

private:
    void onInitialized() {
        
        success_count++;
        
        // Test 2: List available models
        testListModels();
        
        // Test 3: Test local model
        void*::singleShot(1000, this, &ModelRouterTester::testLocalModel);
        
        // Test 4: Test cloud model (if API keys available)
        void*::singleShot(3000, this, &ModelRouterTester::testCloudModel);
        
        // Test 5: Test statistics
        void*::singleShot(5000, this, &ModelRouterTester::testStatistics);
        
        // Test 6: Test model selection
        void*::singleShot(6000, this, &ModelRouterTester::testModelSelection);
        
        // Final report
        void*::singleShot(8000, this, &ModelRouterTester::printReport);
    }

    void onGenerationComplete(const std::string& result, int tokens, double latency) {


        success_count++;
    }

    void onGenerationError(const std::string& error) {
        
        failure_count++;
    }

    void testListModels() {
        test_count++;


        std::vector<std::string> models = model_interface->getAvailableModels();
        if (models.empty()) {
            
            failure_count++;
            return;
        }


        for (const std::string& model : models) {
            
        }
        success_count++;
    }

    void testLocalModel() {
        test_count++;


        std::string prompt = "What is the capital of France? Answer in one sentence.";


        try {
            GenerationResult result = model_interface->generate(prompt, "quantumide-q4km");
            
            if (result.success) {


                success_count++;
            } else {
                
                failure_count++;
            }
        } catch (const std::exception& e) {
            
            failure_count++;
        }
    }

    void testCloudModel() {
        test_count++;


        // Check if OpenAI API key is set
        std::string api_key = qEnvironmentVariable("OPENAI_API_KEY");
        if (api_key.empty()) {


            return;
        }
        
        std::string prompt = "Say 'Hello from cloud model' in exactly 5 words.";


        try {
            GenerationResult result = model_interface->generate(prompt, "gpt-3.5-turbo");
            
            if (result.success) {


                success_count++;
            } else {
                
                failure_count++;
            }
        } catch (const std::exception& e) {
            
            failure_count++;
        }
    }

    void testStatistics() {
        test_count++;


        double avg_latency = model_interface->getAverageLatency();
        int success_rate = model_interface->getSuccessRate();
        double total_cost = model_interface->getTotalCost();
        int request_count = model_interface->getRequestCount();


        success_count++;
    }

    void testModelSelection() {
        test_count++;


        std::string selected = model_interface->selectBestModel("general", "en", true);


        if (!selected.empty()) {
            success_count++;
        } else {
            
            failure_count++;
        }
    }

    void printReport() {


        if (failure_count == 0) {
            
        } else {
            
        }
        
        QCoreApplication::quit();
    }

private:
    std::unique_ptr<ModelInterface> model_interface;
    int test_count;
    int success_count;
    int failure_count;
};

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);


    ModelRouterTester tester;
    
    // Start tests after event loop starts
    void*::singleShot(100, &tester, &ModelRouterTester::runTests);
    
    return app.exec();
}

// MOC removed


