/**
NAMESPACE 'model';
SQLCH 'mysqlch';
**/

---DEFINE DATABASE Auth;

---VTYPE id 'uint32_t';
CREATE TABLE UserMaster(
        id INTEGER PRIMARY KEY
    ,uname VARCHAR
);

CREATE INDEX UserName_Index On UserMaster(uname);

---END DATABASE;

---DEFINE INTERFACE UserRW ON UserMaster;
INSERT INTO UserMaster(uname) VALUES(:uname);
---END INTERFACE;

---DEFINE INTERFACE UserRO ON UserMaster;
SELECT * FROM UserMaster ORDER BY id;
---END INTERFACE;
