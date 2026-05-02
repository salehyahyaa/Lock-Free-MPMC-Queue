#pragma once
#include <pqxx/pqxx>

class Connection {
private:
    pqxx::connection conn;

public:
    Connection();
    pqxx::connection& getConnection();
};


