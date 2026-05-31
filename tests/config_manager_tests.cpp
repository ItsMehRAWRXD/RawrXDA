// ============================================================================
// Configuration Manager Tests — Secure Configuration Testing
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include "../src/config/configuration_manager.cpp"

using namespace RawrXD::Config;

// Mock Session Manager
class MockConfigSessionManager : public Core::SessionManager {
public:
    void SetValue(const std::string& key, const std::string& value) override {
        m_values[key] = value;
    }
    
    std::string GetValue(const std::string& key) override {
        auto it = m_values.find(key);
        return it != m_values.end() ? it->second : "";
    }
    
private:
    std::map<std::string, std::string> m_values;
};

TEST_CASE("Configuration Manager - Basic Operations", "[config][management]") {
    auto sessionManager = std::make_shared<MockConfigSessionManager>();
    ConfigurationManager config(sessionManager);
    
    SECTION("Default configuration") {
        // Initially no config loaded
        REQUIRE(config.GetValue("test_key", Environment::DEVELOPMENT).empty());
    }
    
    SECTION("Set and get value") {
        config.SetValue("api_url", "https://api.example.com", 
                       Environment::DEVELOPMENT, "test_user");
        
        auto value = config.GetValue("api_url", Environment::DEVELOPMENT);
        REQUIRE(value == "https://api.example.com");
    }
    
    SECTION("Environment isolation") {
        config.SetValue("db_host", "dev-db.example.com", 
                       Environment::DEVELOPMENT, "test_user");
        config.SetValue("db_host", "prod-db.example.com", 
                       Environment::PRODUCTION, "test_user");
        
        REQUIRE(config.GetValue("db_host", Environment::DEVELOPMENT) == "dev-db.example.com");
        REQUIRE(config.GetValue("db_host", Environment::PRODUCTION) == "prod-db.example.com");
    }
}

TEST_CASE("Configuration Manager - Secrets Management", "[config][security]") {
    auto sessionManager = std::make_shared<MockConfigSessionManager>();
    ConfigurationManager config(sessionManager);
    
    SECTION("Secret encryption") {
        config.SetSecret("api_key", "secret123", 
                        Environment::PRODUCTION, "admin");
        
        auto secret = config.GetSecret("api_key", Environment::PRODUCTION);
        REQUIRE(secret == "secret123");
    }
    
    SECTION("Secret isolation") {
        config.SetSecret("password", "dev_password", 
                        Environment::DEVELOPMENT, "admin");
        config.SetSecret("password", "prod_password", 
                        Environment::PRODUCTION, "admin");
        
        REQUIRE(config.GetSecret("password", Environment::DEVELOPMENT) == "dev_password");
        REQUIRE(config.GetSecret("password", Environment::PRODUCTION) == "prod_password");
    }
}

TEST_CASE("Configuration Manager - Schema Validation", "[config][validation]") {
    auto sessionManager = std::make_shared<MockConfigSessionManager>();
    ConfigurationManager config(sessionManager);
    
    SECTION("Schema definition") {
        ConfigSchema schema;
        schema.key = "port";
        schema.type = ConfigValueType::INTEGER;
        schema.required = true;
        schema.validationRegex = "^[0-9]+$";
        
        config.DefineSchema(schema);
        
        // Valid value
        config.SetValue("port", "8080", Environment::DEVELOPMENT, "test_user");
        REQUIRE(config.ValidateConfiguration(Environment::DEVELOPMENT) == true);
    }
}

TEST_CASE("Configuration Manager - Snapshots", "[config][snapshots]") {
    auto sessionManager = std::make_shared<MockConfigSessionManager>();
    ConfigurationManager config(sessionManager);
    
    SECTION("Capture and restore snapshot") {
        config.SetValue("setting1", "value1", Environment::DEVELOPMENT, "user1");
        config.SetValue("setting2", "value2", Environment::DEVELOPMENT, "user1");
        
        auto snapshot = config.CaptureSnapshot(Environment::DEVELOPMENT, "admin");
        REQUIRE(snapshot.environment == Environment::DEVELOPMENT);
        REQUIRE(snapshot.values.size() == 2);
        
        // Modify config
        config.SetValue("setting1", "modified", Environment::DEVELOPMENT, "user2");
        
        // Restore snapshot
        config.RestoreSnapshot(snapshot);
        REQUIRE(config.GetValue("setting1", Environment::DEVELOPMENT) == "value1");
    }
}
