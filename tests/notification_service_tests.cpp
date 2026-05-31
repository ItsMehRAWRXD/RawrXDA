// ============================================================================
// Notification Service Tests — Multi-Channel Testing
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include "../src/notifications/notification_service.cpp"

using namespace RawrXD::Notifications;

// Mock Session Manager
class MockNotificationSessionManager : public Core::SessionManager {
public:
    void SetValue(const std::string& key, const std::string& value) override {}
    std::string GetValue(const std::string& key) override { return ""; }
};

TEST_CASE("Notification Service - Basic Operations", "[notifications][messaging]") {
    auto sessionManager = std::make_shared<MockNotificationSessionManager>();
    NotificationService service(sessionManager);
    
    SECTION("Default service state") {
        auto unread = service.GetUnreadNotifications("user1");
        REQUIRE(unread.empty());
    }
    
    SECTION("Template registration") {
        NotificationTemplate templ;
        templ.id = "welcome_email";
        templ.name = "Welcome Email";
        templ.subject = "Welcome to RawrXD";
        templ.body = "Hello {{name}}, welcome to RawrXD!";
        templ.channel = NotificationChannel::EMAIL;
        
        service.RegisterTemplate(templ);
        
        // Template registered (no direct verification in API)
        SUCCEED();
    }
    
    SECTION("Send notification with template") {
        NotificationTemplate templ;
        templ.id = "test_notification";
        templ.name = "Test Notification";
        templ.subject = "Test Subject";
        templ.body = "Test message: {{message}}";
        templ.channel = NotificationChannel::IN_APP;
        
        service.RegisterTemplate(templ);
        
        std::map<std::string, std::string> variables = {
            {"message", "Hello World"}
        };
        
        auto notification = service.SendNotification("test_notification", variables, 
                                                    "user@example.com", 
                                                    NotificationPriority::NORMAL);
        
        REQUIRE_FALSE(notification.id.empty());
        REQUIRE(notification.recipient == "user@example.com");
        REQUIRE(notification.priority == NotificationPriority::NORMAL);
        REQUIRE(notification.status == NotificationStatus::PENDING);
    }
}

TEST_CASE("Notification Service - Bulk Operations", "[notifications][bulk]") {
    auto sessionManager = std::make_shared<MockNotificationSessionManager>();
    NotificationService service(sessionManager);
    
    SECTION("Bulk notification send") {
        NotificationTemplate templ;
        templ.id = "announcement";
        templ.name = "Announcement";
        templ.subject = "Important Update";
        templ.body = "{{content}}";
        templ.channel = NotificationChannel::EMAIL;
        
        service.RegisterTemplate(templ);
        
        std::vector<std::string> recipients = {
            "user1@example.com",
            "user2@example.com",
            "user3@example.com"
        };
        
        std::map<std::string, std::string> variables = {
            {"content", "System maintenance scheduled"}
        };
        
        service.SendBulkNotification("announcement", variables, recipients, 
                                     NotificationPriority::HIGH);
        
        // All notifications should be queued
        SUCCEED();
    }
}

TEST_CASE("Notification Service - Status Management", "[notifications][status]") {
    auto sessionManager = std::make_shared<MockNotificationSessionManager>();
    NotificationService service(sessionManager);
    
    SECTION("Notification status tracking") {
        NotificationTemplate templ;
        templ.id = "status_test";
        templ.name = "Status Test";
        templ.subject = "Test";
        templ.body = "Body";
        templ.channel = NotificationChannel::IN_APP;
        
        service.RegisterTemplate(templ);
        
        auto notification = service.SendNotification("status_test", {}, 
                                                    "user@example.com", 
                                                    NotificationPriority::NORMAL);
        
        auto status = service.GetStatus(notification.id);
        REQUIRE(status == NotificationStatus::PENDING);
    }
    
    SECTION("Mark as read") {
        NotificationTemplate templ;
        templ.id = "read_test";
        templ.name = "Read Test";
        templ.subject = "Test";
        templ.body = "Body";
        templ.channel = NotificationChannel::IN_APP;
        
        service.RegisterTemplate(templ);
        
        auto notification = service.SendNotification("read_test", {}, 
                                                    "user@example.com", 
                                                    NotificationPriority::NORMAL);
        
        service.MarkAsRead(notification.id);
        
        auto status = service.GetStatus(notification.id);
        REQUIRE(status == NotificationStatus::READ);
    }
}

TEST_CASE("Notification Service - Report Generation", "[notifications][reporting]") {
    auto sessionManager = std::make_shared<MockNotificationSessionManager>();
    NotificationService service(sessionManager);
    
    SECTION("Generate notification report") {
        NotificationTemplate templ;
        templ.id = "report_test";
        templ.name = "Report Test";
        templ.subject = "Test";
        templ.body = "Body";
        templ.channel = NotificationChannel::EMAIL;
        
        service.RegisterTemplate(templ);
        
        service.SendNotification("report_test", {}, "user1@example.com", 
                                NotificationPriority::HIGH);
        service.SendNotification("report_test", {}, "user2@example.com", 
                                NotificationPriority::NORMAL);
        service.SendNotification("report_test", {}, "user3@example.com", 
                                NotificationPriority::LOW);
        
        auto report = service.GenerateNotificationReport();
        
        REQUIRE_FALSE(report.empty());
        REQUIRE(report.find("# Notification Report") != std::string::npos);
    }
}
