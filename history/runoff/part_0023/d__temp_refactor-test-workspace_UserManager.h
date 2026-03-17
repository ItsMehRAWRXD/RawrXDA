// UserManager.h - User management with integer IDs
#pragma once

#include <string>

class UserManager {
public:
    UserManager(int id, const std::string& name);
    ~UserManager();
    
    int getId() const { return m_id; }
    std::string getName() const { return m_name; }
    
    void setName(const std::string& name);
    bool validateId() const;
    
private:
    int m_id;  // TODO: Replace with UUID
    std::string m_name;
};
