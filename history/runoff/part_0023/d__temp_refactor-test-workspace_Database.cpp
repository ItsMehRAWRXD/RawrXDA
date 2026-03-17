// Database.cpp - Database operations
#include "UserManager.h"
#include <map>
#include <iostream>

class Database {
public:
    void saveUser(int userId, const std::string& name) {
        std::cout << "Saving user ID: " << userId << std::endl;
        users_[userId] = name;
    }
    
    std::string getUser(int userId) {
        return users_[userId];
    }
    
    bool deleteUser(int userId) {
        std::cout << "Deleting user ID: " << userId << std::endl;
        return users_.erase(userId) > 0;
    }
    
private:
    std::map<int, std::string> users_;
};

int main() {
    Database db;
    UserManager user(12345, "Alice");
    
    db.saveUser(user.getId(), user.getName());
    std::cout << "User saved: " << db.getUser(user.getId()) << std::endl;
    
    return 0;
}
