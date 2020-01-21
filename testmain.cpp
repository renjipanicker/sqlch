#include <iostream>
#include <assert.h>
#include "test.hpp"

inline void createDB(const std::string& filename) {
    model::Auth db;
    db.create(filename);
}

inline void insertDB(const std::string& filename) {
    model::Auth db;
    db.openrw(filename);

    model::UserRW rw(db);
    rw.insertUserMaster("amitabh");
}

inline void selectDB(const std::string& filename) {
    model::Auth db;
    db.openro(filename);

    model::UserRO ro(db);
    auto ul = ro.selectUserMaster();
    assert(ul.size() == 1);
    assert(ul.at(0).uname == "amitabh");
}

int main(int argc, char* argv[]) {
    std::string filename = "test.db";
    createDB(filename);
    insertDB(filename);
    selectDB(filename);
    return 0;
}
