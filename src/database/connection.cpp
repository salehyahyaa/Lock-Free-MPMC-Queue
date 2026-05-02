#include "connection.h"
#include <iostream>

Connection::Connection()
    : conn("dbname=queue_benchmark user=salehyahya host=localhost port=5432")
{
    if (conn.is_open()) {
        std::cout << "Connected to " << conn.dbname() << std::endl;
    }
}

pqxx::connection& Connection::getConnection() {
    return conn;
}