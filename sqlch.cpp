#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <assert.h>
#include <sqlite3.h>

#define SQLCH_TRACE 0

namespace {
    struct Variable {
        std::string name;
        std::string ctype;
        std::string ntype;
        inline Variable(const std::string& n, const std::string& c, const std::string& t)
            : name(n)
            , ctype(c)
            , ntype(t) {}
    };

    struct Column {
        std::string tname;
        std::string cname;
        std::string stype;
        std::string ctype;
        std::string ntype;
        bool is_pk;
        inline Column(const std::string& tn, const std::string& cn, const std::string& s, const std::string& c, const std::string& n, const bool& p)
            : tname(tn)
            , cname(cn)
            , stype(s)
            , ctype(c)
            , ntype(n)
            , is_pk(p) {}
    };

    struct Statement {
        int action;
        std::string sqls;
        std::string tname;

        /// \brief this is the table name for a select stmt which queries one table
        /// it is blank for non-select queries and select-queries spanning multiple tables
        std::string sname;

        std::vector<Column> colList;
        std::vector<Variable> varList;
        std::string qname_;
        std::string pktype_;
        inline Statement(const int& a, const std::string& s)
            : action(a)
            , sqls(s) {}

        inline auto& addColumn(const std::string& tn, const std::string& cn, const std::string& s, const std::string& c, const std::string& n, const bool& p) {
            colList.emplace_back(tn, cn, s, c, n, p);
            return colList.back();
        }

        inline auto& addVariable(const std::string& n, const std::string& t, const std::string& y) {
            varList.emplace_back(n, t, y);
            return varList.back();
        }

        inline auto& qname() const { return qname_; }
        inline auto& pktype() const { return pktype_; }

        inline void finalize(const std::string& qname);
    };

    struct Database;
    struct Module;

    struct Interface {
        Module& module;
        Database& db;
        bool isDB;
        std::string name;
        std::vector<Statement> stmtList;
        inline Interface(Module& m, Database& d, const bool& idb, const std::string& n)
            : module(m), db(d)
            , isDB(idb)
            , name(n) {}

        inline auto& addStatement(const int& action, const std::string& s) {
            stmtList.emplace_back(action, s);
            auto& ls = stmtList.back();
            while((ls.sqls.length() % 8) != 0) {
                ls.sqls += " ";
            }
            return ls;
        }
        inline void generateEncString(const Module& module, const Statement& stmt, std::ostream& of_hdr, std::ostream& of_src, const std::string& ns) const;
        inline void generateCreateTable(const Statement& stmt, std::ostream& of_hdr) const;
        inline void generateInsert(const Statement& stmt, std::ostream& of_hdr, std::ostream& of_src, const std::string& ns) const;
        inline void generateDelete(const Statement& stmt, std::ostream& of_hdr, std::ostream& of_src, const std::string& ns) const;
        inline void generateSelect(const Module& module, const Statement& stmt, std::ostream& of_hdr, std::ostream& of_src, const std::string& ns) const;
        inline void generateIfaceDecl(const Module& module, std::ostream& of_hdr, std::ostream& of_src, const std::string& ns) const;
        inline void generate(const Module& module, std::ostream& of_hdr, std::ostream& of_src, const std::string& ns) const;
    };

    struct Database {
        std::vector<Interface> interfaceList;
        inline auto& addInterface(Module& module, const bool& idb, const std::string& name) {
            interfaceList.emplace_back(module, *this, idb, name);
            return interfaceList.back();
        }

        inline Interface& db() {
            assert(interfaceList.size() > 0);
            return interfaceList.front();
        }

        inline Interface& iface() {
            assert(interfaceList.size() > 0);
            return interfaceList.back();
        }
    };

    struct EnumType {
        std::string name;
        std::vector<std::string> valueList;
        inline EnumType(const std::string& n)
            : name(n) {}
    };

    struct Module {
        std::string name;
        bool generateBase;
        std::string generateBaseNS;
        std::string onError;
        std::string onTrace;
        std::string onOpen;
        std::string onOpened;
        std::string decSql;
        std::string mutexName;
        std::vector<std::string> includeList;
        std::vector<std::string> importList;
        std::vector<std::string> nsList;
        std::vector<EnumType> enumList;
        std::string hcode;
        std::string scode;
        std::vector<Database> dbList;
        bool isAutoIncrement;
        inline Module(const std::string& n)
            : name(n)
            , generateBase(true)
            , generateBaseNS("sqlch")
            , onError("on_Error")
            , onOpen("on_Open")
            , onOpened("on_Opened")
            , decSql("")
            , isAutoIncrement(true) {}

        inline auto& addEnumType(const std::string& name) {
            enumList.emplace_back(name);
            return enumList.back();
        }

        inline auto& db() {
            assert(dbList.size() > 0);
            return dbList.back();
        }

        inline auto& addDatabase(const std::string& name) {
            dbList.emplace_back();
            db().addInterface(*this, true, name);
            return db();
        }

        inline Statement& getCreateStatement(const std::string& tname) {
            assert(tname.length() > 0);
            for(auto& s : db().db().stmtList) {
                if((s.action == SQLITE_CREATE_TABLE) && (s.tname == tname)) {
                    return s;
                }
            }
            std::cout << "Error:Unable to get create statement for table:" << tname << std::endl;
            exit(1);
        }

        inline Column& getColumnInfo(const std::string& tname, const std::string& cname) {
            auto& cs = getCreateStatement(tname);
            for(auto& c : cs.colList) {
                if(c.cname == cname) {
                    return c;
                }
            }
            std::cout << "Error:Unable to get type for column:" << cname << ", table:" << tname << std::endl;
            exit(1);
        }

        inline void finalize(Statement& s, const std::string& qname) {
            s.finalize(qname);
            if((s.action == SQLITE_INSERT) || (s.action == SQLITE_UPDATE)) {
                auto& cs = getCreateStatement(s.tname);
                s.pktype_ = "uint64_t";
                for(auto& c : cs.colList) {
                    if(c.is_pk) {
                        s.pktype_ = c.ntype;
                        break;
                    }
                }
            }
        }
    };

    inline std::string error(sqlite3* db) {
        return ::sqlite3_errmsg(db);
    }

    inline std::string param(const char* p) {
        if(p == 0) {
            return "";
        }
        return p;
    }

    struct Parser {
        Module& module;
        sqlite3* db;

        int last_actioncode;
        std::string primary_table;
        std::map<std::string, std::string> cmap;
        std::map<std::string, std::string> nmap;
        std::string qname;
        std::string limit;
        std::string offset;

        inline int authcb(int actioncode, const std::string& p3, const std::string& /*p4*/, const std::string& /*p5*/, const std::string& /*p6*/) {
#if SQLCH_TRACE
            std::cout
                << "AC_CODE:" << actioncode
                << ", LAST_AC_CODE:" << last_actioncode
                << ", p3:" << p3
                //<< ", p4:" << p4
                //<< ", p5:" << p5
                //<< ", p6:" << p6
                << std::endl;
#endif
            if(actioncode == SQLITE_CREATE_TABLE) {
                assert(last_actioncode == 0);
                last_actioncode = actioncode;
                primary_table = p3;
            } else if(actioncode == SQLITE_CREATE_INDEX) {
                if(p3.substr(0,7) != "sqlite_") {
                    assert((last_actioncode == 0) || (last_actioncode == SQLITE_CREATE_TABLE));
                    last_actioncode = actioncode;
                    primary_table = p3;
                }
            } else if(actioncode == SQLITE_INSERT) {
                if(p3.substr(0,7) != "sqlite_") {
                    assert(last_actioncode == 0);
                    last_actioncode = actioncode;
                    primary_table = p3;
                }
            } else if(actioncode == SQLITE_UPDATE) {
                if(p3.substr(0,7) != "sqlite_") {
                    assert((last_actioncode == 0) || (last_actioncode == actioncode));
                    last_actioncode = actioncode;
                    primary_table = p3;
                }
            } else if(actioncode == SQLITE_DELETE) {
                if(p3.substr(0,7) != "sqlite_") {
                    assert(last_actioncode == 0);
                    last_actioncode = actioncode;
                    primary_table = p3;
                }
            } else if(actioncode == SQLITE_SELECT) {
                assert(last_actioncode == 0);
                last_actioncode = actioncode;
            } else if(actioncode == SQLITE_READ) {
                // no-op
            } else if(actioncode == SQLITE_PRAGMA) {
                // no-op
            } else if(actioncode == SQLITE_FUNCTION) {
                // no-op
            } else if(actioncode == SQLITE_REINDEX) {
                // no-op
            } else {
                assert(last_actioncode == 0);
                last_actioncode = 0;
                primary_table = "";
            }
            return SQLITE_OK;
        }

        static int authcb(void* udf, int actioncode, const char* p3, const char* p4, const char* p5, const char* p6) {
            Parser* d = (Parser*)udf;
            return d->authcb(actioncode, param(p3), param(p4), param(p5), param(p6));
        }

        inline std::string getType(const std::string& t) const {
            auto cit = cmap.find(t);
            if(cit != cmap.end()) {
                return cit->second;
            }
            std::cout << "Error:Unknown type:" << t << std::endl;
            exit(1);
        }

        inline std::string getNativeType(const std::string& v, const std::string& s) const {
            auto vit = nmap.find(v);
            if(vit != nmap.end()) {
                return vit->second;
            }
            return s;
        }

        inline bool addVariableByMap(Statement& s, const std::string& v) const {
            auto vit = nmap.find(v);
            if(vit != nmap.end()) {
                auto& ctype = vit->second;
                s.addVariable(v, ctype, ctype);
                return true;
            }
            return false;
        }

        inline bool addVariableByTable(Statement& s, const std::string& tname, const std::string& v, const bool& exact) const {
            auto& db = module.db();
            for(auto& d : db.db().stmtList) {
                if((d.action == SQLITE_CREATE_TABLE) && (d.tname == tname)) {
                    for(auto& c : d.colList) {
                        auto match = (c.cname == v);
                        if(!match && !exact) {
                            match = (v.compare(0, c.cname.size(), c.cname) == 0);
                        }
                        if(match) {
                            s.addVariable(v, c.ctype, c.ntype);
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        inline void addVariable(Statement& s, const std::string& v) const {
            if(addVariableByMap(s, v)) {
                return;
            }
            if(s.action == SQLITE_SELECT) {
                if((":" + v) == limit) {
                    s.addVariable(v, "int64_t", "int64_t");
                    return;
                }
                if((":" + v) == offset) {
                    s.addVariable(v, "int64_t", "int64_t");
                    return;
                }
                for(auto& c : s.colList) {
                    if(addVariableByTable(s, c.tname, v, true) || addVariableByTable(s, c.tname, v, false)) {
                        return;
                    }
                }
                std::cout << "Error:Unable to get type for select-variable:" << v << std::endl;
                exit(1);
            }
            if(addVariableByTable(s, s.tname, v, true) || addVariableByTable(s, s.tname, v, false)) {
                return;
            }
            std::cout << "Error:Unable to get type for variable:" << v << std::endl;
            exit(1);
        }

        inline void setColumnInfo(Statement& s);

        inline Parser(Module& m)
            : module(m)
            , db(nullptr)
            , last_actioncode(0) {
            int rv = ::sqlite3_open_v2(":memory:", &db, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, 0);
            if(rv != SQLITE_OK) {
                std::cout << "Error:Unable to open in-memory Parser" << std::endl;
                exit(1);
            }
            rv = sqlite3_set_authorizer(db, authcb, this);
            if(rv != SQLITE_OK) {
                std::cout << "Error:Unable to set authorization callback:" << error(db) << std::endl;
                exit(1);
            }

            cmap["INTEGER"] = "int64_t";
            cmap["VARCHAR"] = "std::string";
            cmap["TEXT"] = "std::string";
            cmap["JSON"] = "std::string";
        }

        inline ~Parser() {
            if(db != nullptr) {
                sqlite3_close(db);
            }
        }

        inline void reset() {
            last_actioncode = 0;
            primary_table = "";
            nmap.clear();
            qname = "";
            limit = "";
            offset = "";
        }
    };

    struct Cursor {
        Parser& parser;
        sqlite3_stmt* stmt;
        inline Cursor(Parser& p)
            : parser(p)
            , stmt(nullptr) {}

        inline ~Cursor() {
            if(stmt != nullptr) {
                ::sqlite3_reset(stmt);
                ::sqlite3_clear_bindings(stmt);
                ::sqlite3_finalize(stmt);
            }
        }

        inline void open(const std::string& sql) {
            int rc = ::sqlite3_prepare_v2(parser.db, sql.c_str(), sql.length(), &(stmt), 0);
            if(rc != SQLITE_OK) {
                std::cout << "Error:Unable to prepare statement:" << sql << "[" << error(parser.db) << "]" << std::endl;
                exit(1);
            }
            if(parser.limit.length() > 0) {
                int idx = ::sqlite3_bind_parameter_index(stmt, parser.limit.c_str());
                if(idx == 0) {
                    std::cout << "unknown limit name:" << parser.limit << std::endl;
                    exit(1);
                }
                ::sqlite3_bind_int64(stmt, idx, 1);
            }
            if(parser.offset.length() > 0) {
                int idx = ::sqlite3_bind_parameter_index(stmt, parser.offset.c_str());
                if(idx == 0) {
                    std::cout << "unknown offset name:" << parser.offset << std::endl;
                    exit(1);
                }
                ::sqlite3_bind_int64(stmt, idx, 1);
            }
        }

        inline bool next() {
            int rc = ::sqlite3_step(stmt);
            if((rc > 0) && (rc < 100)) {
                std::cout << "Error:Unable to exec statement:" << rc << "[" << error(parser.db) << "]" << std::endl;
                exit(1);
            }
            return (rc == SQLITE_ROW);
        }
    };

    inline void Parser::setColumnInfo(Statement& s) {
        Cursor cursor(*this);
        cursor.open("PRAGMA table_info('" + s.tname + "')");
        while(cursor.next()) {
            std::string cname = (const char*)sqlite3_column_text(cursor.stmt, 1);
            std::string stype = (const char*)sqlite3_column_text(cursor.stmt, 2);
            int ispk = sqlite3_column_int(cursor.stmt, 5);
            auto ctype = getType(stype);
            auto ntype = getNativeType(cname, ctype);
            s.addColumn(s.tname, cname, stype, ctype, ntype, (ispk != 0));
        }
    }

    inline void Statement::finalize(const std::string& qname) {
        auto n = qname;
        if(n.length() == 0) {
            switch(action) {
            case SQLITE_CREATE_TABLE:
                n = "create";
                n += tname;
                break;
            case SQLITE_CREATE_INDEX:
                n = "create";
                n += tname;
                break;
            case SQLITE_INSERT:
                n = "insert";
                n += tname;
                break;
            case SQLITE_UPDATE:
                n = "update";
                n += tname;
                for(auto& v : varList) {
                    n += "_";
                    n += v.name;
                }
                break;
            case SQLITE_DELETE:
                n = "delete";
                n += tname;
                for(auto& v : varList) {
                    n += "_";
                    n += v.name;
                }
                break;
            case SQLITE_SELECT:
                n = "select";
                n += tname;
                for(auto& v : varList) {
                    n += "_";
                    n += v.name;
                }
                break;
            }
        }
        qname_ = n;
    }

    inline bool processMetaStatement(Parser& parser, const std::vector<std::string>& tokList) {
        if(tokList.at(0) == "INCLUDE") {
            auto hdr = tokList.at(1);
            parser.module.includeList.push_back(hdr);
            return true;
        }
        if(tokList.at(0) == "IMPORT") {
            auto hdr = tokList.at(1);
            parser.module.importList.push_back(hdr);
            return true;
        }
        if(tokList.at(0) == "SQLCH") {
            parser.module.generateBase = (tokList.at(1) != "OFF");
            return true;
        }
        if(tokList.at(0) == "SQLCH_NS") {
            parser.module.generateBaseNS = tokList.at(1);
            return true;
        }
        if(tokList.at(0) == "DECSQL") {
            parser.module.decSql = tokList.at(1);
            return true;
        }
        if(tokList.at(0) == "MUTEX") {
            auto name = tokList.at(1);
            parser.module.mutexName = name;
            return true;
        }
        if(tokList.at(0) == "ON") {
            auto what = tokList.at(1);
            auto func = tokList.at(2);
            if(parser.module.generateBase == false) {
                std::cout << "Error:ON " << what << " cannot be defined when SQLCH is OFF" << std::endl;
                exit(1);
            }
            if(what == "ERROR") {
                parser.module.onError = func;
            }
            if(what == "TRACE") {
                parser.module.onTrace = func;
            }
            if(what == "OPEN") {
                parser.module.onOpen = func;
            }
            if(what == "OPENED") {
                parser.module.onOpened = func;
            }
            return true;
        }
        if(tokList.at(0) == "NAMESPACE") {
            auto ns = tokList.at(1);
            auto npos = ns.find("::");
            while(npos != std::string::npos) {
                auto n = ns.substr(0, npos);
                parser.module.nsList.push_back(n);
                ns = ns.substr(npos + 2);
                npos = ns.find("::");
            }
            parser.module.nsList.push_back(ns);
            return true;
        }
        if(tokList.at(0) == "ENUM") {
            auto n = tokList.at(1);
            auto& et = parser.module.addEnumType(n);
            size_t idx = 2;
            if(tokList.at(idx) != "(") {
            }
            ++idx;
            while((idx < tokList.size()) && (tokList.at(idx) != ")")) {
                et.valueList.push_back(tokList.at(idx));
                ++idx;
            }
            if(idx == tokList.size()) {
            }
            return true;
        }
        if(tokList.at(0) == "HCODE") {
            parser.module.hcode += tokList.at(1);
            return true;
        }
        if(tokList.at(0) == "SCODE") {
            parser.module.scode += tokList.at(1);
            return true;
        }
        if(tokList.at(0) == "VTYPE") {
            const auto& vname = tokList.at(1);
            const auto& tname = tokList.at(2);
            parser.nmap[vname] = tname;
            return true;
        }
        if(tokList.at(0) == "AUTOINCREMENT") {
            auto& state = tokList.at(1);
            if(state == "OFF"){
                parser.module.isAutoIncrement = false;
            }else{
                parser.module.isAutoIncrement = true;
            }
            return true;
        }
        if(tokList.at(0) == "QNAME") {
            parser.qname = tokList.at(1);
            return true;
        }
        if(tokList.at(0) == "LIMIT") {
            parser.limit = ":" + tokList.at(1);
            return true;
        }
        if(tokList.at(0) == "OFFSET") {
            parser.offset = ":" + tokList.at(1);
            return true;
        }
        if(tokList.at(0) == "DEFINE") {
            if(tokList.at(1) == "DATABASE") {
                parser.module.addDatabase(tokList.at(2));
            }
            if(tokList.at(1) == "INTERFACE") {
                parser.module.db().addInterface(parser.module, false, tokList.at(2));
            }
            return true;
        }
        if(tokList.at(0) == "END") {
            return true;
        }
        std::cout << "unhandled metacommand:"
                  << "[";
        for(auto& t : tokList) {
            std::cout << t << " ";
        }
        std::cout << "]" << std::endl;
        return true;
    }

    inline void processSqlStatement(Parser& parser, const std::string& sql) {
#if SQLCH_TRACE
        std::cout << "processStatement:" << sql << std::endl;
#endif
        Cursor cursor(parser);
        cursor.open(sql);
        cursor.next();
        if(parser.last_actioncode == SQLITE_CREATE_TABLE) {
            auto& s = parser.module.db().db().addStatement(parser.last_actioncode, sql);
            s.tname = parser.primary_table;
            parser.setColumnInfo(s);
            parser.module.finalize(s, parser.qname);
            parser.reset();
        } else if(parser.last_actioncode == SQLITE_CREATE_INDEX) {
            auto& s = parser.module.db().db().addStatement(parser.last_actioncode, sql);
            s.tname = parser.primary_table;
            parser.setColumnInfo(s);
            parser.module.finalize(s, parser.qname);
            parser.reset();
        } else if(parser.last_actioncode != 0) {
            auto& s = parser.module.db().iface().addStatement(parser.last_actioncode, sql);
            s.tname = parser.primary_table;

            int cc = sqlite3_column_count(cursor.stmt);
            for(int i = 0; i < cc; ++i) {
                const char* tx = sqlite3_column_table_name(cursor.stmt, i);
                std::string t;
                if(tx != nullptr) {
                    t = sqlite3_column_table_name(cursor.stmt, i);
                }
                std::string c = sqlite3_column_name(cursor.stmt, i);
                auto cn = c;
                auto cit = parser.nmap.find(cn);
                if(cit != parser.nmap.end()) {
                    auto ntype = cit->second;
                    s.addColumn(t, c, ntype, ntype, ntype, false);
                }else {
                    auto& col = parser.module.getColumnInfo(t, cn);
                    s.addColumn(t, c, col.stype, col.ctype, col.ntype, col.is_pk);
                }
                if(s.tname.size() == 0) {
                    s.tname = t;
                }

                std::string dt = parser.module.db().db().name + "::" + t;
                if(s.sname.size() == 0) {
                    s.sname = dt;
                } else {
                    if((s.sname != dt) && (s.sname != "+")) {
                        s.sname = "+";
                    }
                }
            }

            int vc = sqlite3_bind_parameter_count(cursor.stmt);
            for(int i = 0; i < vc; ++i) {
                std::string n = sqlite3_bind_parameter_name(cursor.stmt, i + 1);
                n = n.substr(1); // skip past the initial ":"
                parser.addVariable(s, n);
            }

            parser.module.finalize(s, parser.qname);
            parser.reset();
        }
    }

    inline std::string slurpFile(const std::string& sqlfile) {
        std::ifstream is(sqlfile);
        if(!is.is_open()) {
            std::cout << "Error:Unable to open file:" << sqlfile << std::endl;
            exit(1);
        }

        std::string str;

        is.seekg(0, std::ios::end);
        str.reserve((size_t)is.tellg());
        is.seekg(0, std::ios::beg);

        str.assign((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
        return str;
    }

    enum class LineType {
        Eof,
        SlComment,
        Sql,
        Meta,
        EnterType,
        LeaveType
    };

#if 0
    inline std::ostream& operator<<(std::ostream& os, const LineType& val) {
        switch (val) {
        case LineType::Eof: os << "Eof"; break;
        case LineType::SlComment: os << "SlComment"; break;
        case LineType::Sql: os << "Sql"; break;
        case LineType::Meta: os << "Meta"; break;
        case LineType::EnterType: os << "EnterType"; break;
        case LineType::LeaveType: os << "LeaveType"; break;
        }
        return os;
    }
#endif

    inline bool isWS(std::string::iterator& it, std::string::iterator& ite) {
        if(it == ite) {
            return false;
        }
        switch(*it) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            return true;
        }
        return false;
    }

    inline bool isEOL(std::string::iterator& it, std::string::iterator& ite) {
        if(it == ite) {
            return true;
        }
        return false;
    }

    inline bool isMetaEOL(std::string::iterator& it, std::string::iterator& ite) {
        if(isEOL(it, ite) || (*it == ';')) {
            return true;
        }
        return false;
    }

    inline bool isMetaID(std::string::iterator& it, std::string::iterator& ite) {
        if(isMetaEOL(it, ite) || (isWS(it, ite))) {
            return false;
        }
        int ch = *it;
        if(ch == '_') {
            return true;
        }
        if((ch >= 'a') && (ch <= 'z')) {
            return true;
        }
        if((ch >= 'A') && (ch <= 'Z')) {
            return true;
        }
        if((ch >= '0') && (ch <= '9')) {
            return true;
        }
        return false;
    }

    inline bool isMetaSYM(std::string::iterator& it, std::string::iterator& ite) {
        if(isMetaEOL(it, ite) || (isWS(it, ite))) {
            return false;
        }
        if(isMetaID(it, ite)) {
            return false;
        }
        return true;
    }

    inline LineType readLine(const bool& typemode, std::string::iterator& it, std::string::iterator& ite, std::vector<std::string>& tokList) {
        tokList.clear();
        while(isWS(it, ite)) {
            ++it;
        }
        auto llen = (ite - it);
        if((llen >= 3) && (std::string(it, (it + 3)) == "/**")) {
            it += 3;
            return LineType::EnterType;
        }

        if((llen >= 2) && (std::string(it, (it + 3)) == "/*")) {
            while(it != ite) {
                if((*it == '*') && ((*(it + 1)) == '/')) {
                    it += 2;
                    break;
                }
                ++it;
            }
            return LineType::SlComment;
        }

        if((typemode) && ((llen >= 3) && (std::string(it, (it + 3)) == "**/"))) {
            it += 3;
            return LineType::LeaveType;
        }

        if(typemode || ((llen >= 3) && (std::string(it, (it + 3)) == "---"))) {
            if(!typemode) {
                it += 3;
            }
            while(!isMetaEOL(it, ite)) {
                if(isMetaEOL(it, ite)) {
                    break;
                } else if(isWS(it, ite)) {
                    while((!isMetaEOL(it, ite)) && (isWS(it, ite))) {
                        ++it;
                    }
                } else if((*it == '/') && ((*(it + 1)) == '*')) {
                    while(it != ite) {
                        if((*it == '*') && ((*(it + 1)) == '/')) {
                            it += 2;
                            break;
                        }
                        ++it;
                    }
                } else if(((ite - it) >= 2) && (*it == '-') && ((*(it + 1)) == '-')) {
                    while((it != ite) && (*it != '\n') && (!isEOL(it, ite))) {
                        ++it;
                    }
                } else if(*it == '\'') {
                    ++it;
                    std::string s;
                    while((!isEOL(it, ite)) && (*it != '\'')) {
                        s += *it;
                        ++it;
                    }
                    if(!isEOL(it, ite)) {
                        assert(*it == '\'');
                        ++it;
                    }
                    tokList.push_back(s);
                } else if(isMetaID(it, ite)) {
                    std::string s;
                    while(isMetaID(it, ite)) {
                        s += *it;
                        ++it;
                    }
                    tokList.push_back(s);
                } else if(isMetaSYM(it, ite)) {
                    std::string s;
                    while(isMetaSYM(it, ite)) {
                        s += *it;
                        ++it;
                    }
                    tokList.push_back(s);
                }
            }
            if((!isEOL(it, ite)) && (*it == ';')) {
                ++it;
            }
            return LineType::Meta;
        }

        if((llen >= 2) && (std::string(it, (it + 2)) == "--")) {
            while((it != ite) && (*it != '\r') && (*it != '\n')) {
                ++it;
            }
            return LineType::SlComment;
        }

        std::string line = "";
        while((it != ite) && (*it != ';')) {
            line += *it;
            ++it;
        }
        if(it == ite) {
            return LineType::Eof;
        }
        tokList.push_back(line);
        assert(*it == ';');
        ++it;
        return LineType::Sql;
    }

    inline void readFile(const std::string& sqlfile, Module& module) {
        std::string str = slurpFile(sqlfile);
        Parser parser(module);

        auto it = str.begin();
        auto ite = str.end();
        LineType lt = LineType::Eof;
        std::vector<std::string> tokList;
        bool typemode = false;
        while((lt = readLine(typemode, it, ite, tokList)) != LineType::Eof) {
#if 0
            for (auto t : tokList) {
                if (t.find("ERROR") != std::string::npos) {
                    std::replace(t.begin(), t.end(), 'R', 'X');
                }
                std::cout << "-TOK:" << t << std::endl;
            }
            std::cout << std::endl;
#endif
            switch(lt) {
            case LineType::Eof:
                assert(false);
                break;
            case LineType::Meta:
                processMetaStatement(parser, tokList);
                break;
            case LineType::SlComment:
                break;
            case LineType::EnterType:
                typemode = true;
                break;
            case LineType::LeaveType:
                typemode = false;
                break;
            case LineType::Sql:
                processSqlStatement(parser, tokList.at(0));
                break;
            }
        }
    }

    inline void Interface::generateEncString(const Module& module, const Statement& stmt, std::ostream& /*of_hdr*/, std::ostream& of_src, const std::string& ns) const {
#if ENCRYPTED_SQL_STRING
        const uint32_t kval[] = SQL_ENCRYPTION_KVAL;
        s::EncKey key(kval, SQL_ENCRYPTION_KLEN / sizeof(uint32_t));
        std::string estr;
        if(!s::Encryption::enctext(stmt.sqls, estr, key)) {
            std::cout << "Error encrypting SQL:" << stmt.sqls << std::endl;
            exit(1);
        }
#else
        std::string estr = stmt.sqls;
#endif

        std::ostringstream ss;
        bool inws = false;
        for(auto& c : stmt.sqls) {
            if((c == '\r') || (c == '\n')) {
                if(inws) {
                } else {
                    ss << "\\r\\n\"" << std::endl;
                    inws = true;
                }
            } else {
                if(inws) {
                    ss << "      \"";
                    inws = false;
                } else {
                }
                ss << c;
            }
        }
        of_src << "static inline std::string " << stmt.qname() << "_s() {" << std::endl;
        if(estr == stmt.sqls) {
            of_src << "    return \"" << ss.str() << "\";" << std::endl;
        } else {
            of_src << "/*\n    \"" << ss.str() << "\"\n*/" << std::endl;
            of_src << "    static const unsigned char arr[] = {" << std::endl
                   << "    ";
            for(size_t i = 0; i < estr.length(); ++i) {
                unsigned char ch = estr.at(i);
                of_src << "  0x" << std::hex << std::setw(2) << std::setfill('0') << +ch << std::dec << ",";
                if(((i + 1) % 8) == 0) {
                    of_src << std::endl
                           << "    ";
                }
            }
            of_src << std::endl;
            of_src << "    };" << std::endl;
            if(module.decSql.size() > 0){
                of_src << "    return " << ns << module.decSql << "(std::string((const char*)arr, sizeof(arr)));" << std::endl;
            }
        }

        of_src << "}" << std::endl;
        of_src << std::endl;
    }

    inline void Interface::generateCreateTable(const Statement& stmt, std::ostream& of_hdr) const {
        of_hdr << "      struct " << stmt.tname << " {" << std::endl;
        for(auto& c : stmt.colList) {
            of_hdr << "        " << c.ntype << " " << c.cname << ";" << std::endl;
        }
        of_hdr << "      };" << std::endl;
    }

    inline void Interface::generateInsert(const Statement& stmt, std::ostream& of_hdr, std::ostream& of_src, const std::string& ns) const {
        std::string sep;

        of_hdr << "    " << module.generateBaseNS << "::exstatement " << stmt.qname() << "_;" << std::endl;
        if(module.isAutoIncrement){
            of_hdr << "    " << stmt.pktype() << " " << stmt.qname() << "(";
        }else{
            of_hdr << "    void " << stmt.qname() << "(";
        }
        for(auto& v : stmt.varList) {
            of_hdr << sep << "const " << v.ntype << "& " << v.name;
            sep = ", ";
        }
        of_hdr << ");" << std::endl;
        of_hdr << std::endl;

        auto fqname = ns + name + "::" + stmt.qname();
        sep = "";
        if(module.isAutoIncrement){
            of_src << stmt.pktype() << " " << fqname << "(";
        }else{
            of_src << "void " << fqname << "(";
        }
        for(auto& v : stmt.varList) {
            of_src << sep << "const " << v.ntype << "& " << v.name;
            sep = ", ";
        }
        of_src << ") {" << std::endl;
        of_src << "  " << stmt.qname() << "_.reset();" << std::endl;
        for(auto& v : stmt.varList) {
            auto ctype = "static_cast<" + v.ctype + ">";
            if(v.ntype == v.ctype) {
                ctype = "";
            }
            of_src << "  " << stmt.qname() << "_.setParam<" << v.ctype << ">(\":" << v.name << "\", " << ctype << "(" << v.name << "));" << std::endl;
        }
        of_src << "  return " << stmt.qname() << "_.insert();" << std::endl;
        of_src << "}" << std::endl;
        of_src << std::endl;
    }

    inline void Interface::generateDelete(const Statement& stmt, std::ostream& of_hdr, std::ostream& of_src, const std::string& ns) const {
        std::string sep;

        of_hdr << "    " << module.generateBaseNS << "::exstatement " << stmt.qname() << "_;" << std::endl;
        of_hdr << "    void " << stmt.qname() << "(";
        for(auto& v : stmt.varList) {
            of_hdr << sep << "const " << v.ntype << "& " << v.name;
            sep = ", ";
        }
        of_hdr << ");" << std::endl;
        of_hdr << std::endl;

        auto fqname = ns + name + "::" + stmt.qname();
        sep = "";
        of_src << "void " << fqname << "(";
        for(auto& v : stmt.varList) {
            of_src << sep << "const " << v.ntype << "& " << v.name;
            sep = ", ";
        }
        of_src << ") {" << std::endl;
        of_src << "  " << stmt.qname() << "_.reset();" << std::endl;
        for(auto& v : stmt.varList) {
            auto ctype = "static_cast<" + v.ctype + ">";
            if(v.ntype == v.ctype) {
                ctype = "";
            }
            of_src << "  " << stmt.qname() << "_.setParam<" << v.ctype << ">(\":" << v.name << "\", " << ctype << "(" << v.name << ")" << ");" << std::endl;
        }
        of_src << "  " << stmt.qname() << "_.xdelete();" << std::endl;
        of_src << "}" << std::endl;
        of_src << std::endl;
    }

    inline void Interface::generateSelect(const Module& /*module*/, const Statement& stmt, std::ostream& of_hdr, std::ostream& of_src, const std::string& ns) const {
        std::string sep;
        auto sname = stmt.sname;
        if(sname == "+") {
            sname = "row";
        }

        auto fqname = ns + name + "::" + stmt.qname();
        auto rname = sname;
        if(sname == "row") {
            rname = fqname + "_c::row";
        } else {
            rname = ns + sname;
        }

        of_hdr << "    struct " << stmt.qname() << "_c : public " << module.generateBaseNS << "::statement {" << std::endl;
        of_hdr << "      friend struct " << name << ";" << std::endl;
        if(sname == "row") {
            of_hdr << "      struct row {" << std::endl;
            for(auto& c : stmt.colList) {
                of_hdr << "        " << c.ntype << " " << c.cname << ";" << std::endl;
            }
            of_hdr << "      };" << std::endl;
        }

        of_hdr << "      inline " << stmt.qname() << "_c(" << module.generateBaseNS << "::database& pdb) : statement(pdb) {}" << std::endl;
        of_hdr << "    };" << std::endl;
        of_hdr << "    " << stmt.qname() << "_c " << stmt.qname() << "_;" << std::endl;

        of_hdr << "    "
               << "std::vector<" << rname << "> " << stmt.qname() << "(";

        sep = "";
        for(auto& v : stmt.varList) {
            of_hdr << sep << "const " << v.ntype << "& " << v.name;
            sep = ", ";
        }
        of_hdr << ");" << std::endl;
        of_hdr << std::endl;

        sep = "";
        of_src << "std::vector<" << rname << "> " << fqname << "(";
        for(auto& v : stmt.varList) {
            of_src << sep << "const " << v.ntype << "& " << v.name;
            sep = ", ";
        }
        of_src << ") {" << std::endl;

        of_src << "  " << module.generateBaseNS << "::guard lk(db.db);" << std::endl;
        of_src << "  " << stmt.qname() << "_.reset();" << std::endl;
        for(auto& v : stmt.varList) {
            auto ctype = "static_cast<" + v.ctype + ">";
            if(v.ntype == v.ctype) {
                ctype = "";
            }
            of_src << "  " << stmt.qname() << "_.setParam<" << v.ctype << ">(\":" << v.name << "\", " << ctype << "(" << v.name << ")" << ");" << std::endl;
        }
        of_src << "  std::vector<" << rname << "> rv;" << std::endl;
        of_src << "  while(" << stmt.qname() << "_.next()){" << std::endl;
        of_src << "    rv.push_back(" << rname << "());" << std::endl;
        of_src << "    auto& s = rv.back();" << std::endl;
        size_t idx = 0;
        for(auto& c : stmt.colList) {
            auto ntype = "static_cast<" + c.ntype + ">";
            if(c.ntype == c.ctype) {
                ntype = "";
            }
            of_src << "    s." << c.cname << " = " << ntype << "(" << stmt.qname() << "_.getColumn<" << c.ctype << ">(" << idx << "));" << std::endl;
            ++idx;
        }
        of_src << "  }" << std::endl;
        of_src << "  return rv;" << std::endl;
        of_src << "}" << std::endl;
        of_src << std::endl;
    }


    inline void Interface::generateIfaceDecl(const Module& module, std::ostream& of_hdr, std::ostream& of_src, const std::string& ns) const {
        if (isDB) {
            return;
        }
        of_hdr << "  struct " << name << ";" << std::endl;
    }

    inline void Interface::generate(const Module& module, std::ostream& of_hdr, std::ostream& of_src, const std::string& ns) const {
        of_hdr << "  struct " << name << " {" << std::endl;
        if(isDB) {
            of_hdr << "    " << module.generateBaseNS << "::database db;" << std::endl;
            of_hdr << "    std::string name;" << std::endl;
        } else {
            of_hdr << "    typedef " << module.generateBaseNS << "::pool<" << db.db().name << ", " << name << ">::guard guard;" << std::endl;
            of_hdr << "    " << db.db().name << "& db;" << std::endl;
        }

        for(auto& s : stmtList) {
            switch(s.action) {
            case SQLITE_CREATE_TABLE:
                generateCreateTable(s, of_hdr);
                break;
            case SQLITE_INSERT:
            case SQLITE_UPDATE:
                generateInsert(s, of_hdr, of_src, ns);
                break;
            case SQLITE_DELETE:
                generateDelete(s, of_hdr, of_src, ns);
                break;
            case SQLITE_SELECT:
                generateSelect(module, s, of_hdr, of_src, ns);
                break;
            }
        }

        if(isDB) {
            of_hdr << "    void create(const std::string& filename, const char* vfs = nullptr);" << std::endl;
            of_hdr << "    void openrw(const std::string& filename, const char* vfs = nullptr);" << std::endl;
            of_hdr << "    void openro(const std::string& filename, const char* vfs = nullptr);" << std::endl;
        } else {
            of_hdr << "    void open();" << std::endl;
        }
        of_hdr << "    inline " << name << "& operator=(const " << name << "&) = delete;" << std::endl;
        of_hdr << "    inline " << name << "(const " << name << "&) = delete;" << std::endl;
        of_hdr << "    inline " << name << "& operator=(" << name << "&&) = delete;" << std::endl;
        of_hdr << "    inline " << name << "(" << name << "&&) = delete;" << std::endl;

        std::string sep;
        if(isDB) {
            of_hdr << "    inline " << name << "()";
            sep = ":";
            for (auto& db : module.dbList) {
                for (auto& iface : db.interfaceList) {
                    if (!iface.isDB) {
                        of_hdr << sep << iface.name << "Pool(*this)";
                        sep = ",";
                    }
                }
            }
        } else {
            of_hdr << "    inline " << name << "(" << db.db().name << "& d, const bool& doOpen = true) : db(d)";
            sep = ", ";
        }
        for(auto& s : stmtList) {
            switch(s.action) {
            case SQLITE_CREATE_TABLE:
                break;
            case SQLITE_INSERT:
            case SQLITE_UPDATE:
            case SQLITE_DELETE:
            case SQLITE_SELECT:
                of_hdr << sep << s.qname() << "_(db.db)";
                sep = ", ";
                break;
            }
        }

        if (isDB) {
            of_hdr << " {}" << std::endl;
            for (auto& db : module.dbList) {
                for (auto& iface : db.interfaceList) {
                    if (!iface.isDB) {
                        of_hdr << "    " << module.generateBaseNS << "::pool<" << name << "," << iface.name << "> " << iface.name << "Pool;" << std::endl;
                    }
                }
            }
        }else{
            of_hdr << " {if(doOpen){open();}}" << std::endl;
        }
        of_hdr << "  };" << std::endl;
        of_hdr << std::endl;

        for(auto& s : stmtList) {
            generateEncString(module, s, of_hdr, of_src, ns);
        }

        if(isDB) {
            of_src << "void " << ns << name << "::create(const std::string& filename, const char* vfs) {" << std::endl;
            of_src << "  FILE* fp = ::fopen(filename.c_str(), \"r\");" << std::endl;
            of_src << "  if(fp){" << std::endl;
            of_src << "    ::fclose(fp);" << std::endl;
            of_src << "    ::remove(filename.c_str());" << std::endl;
            of_src << "  }" << std::endl;
            of_src << "  db.create(filename, vfs);" << std::endl;
            of_src << "  db.exec(\"PRAGMA page_size = 4096;\");" << std::endl;
            of_src << "  " << module.generateBaseNS << "::transaction t(db);" << std::endl;
            for(auto& s : stmtList) {
                switch(s.action) {
                case SQLITE_CREATE_TABLE:
                case SQLITE_CREATE_INDEX:
                    of_src << "  db.exec(" << s.qname() << "_s());" << std::endl;
                    break;
                }
            }
            of_src << "  t.commit();" << std::endl;
            of_src << "  if(name.size() == 0){" << std::endl;
            of_src << "    name = filename;" << std::endl;
            of_src << "  }" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;
            of_src << "void " << ns << name << "::openrw(const std::string& filename, const char* vfs) {" << std::endl;
            of_src << "  db.openrw(filename, vfs);" << std::endl;
            of_src << "  if(name.size() == 0){" << std::endl;
            of_src << "    name = filename;" << std::endl;
            of_src << "  }" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;
            of_src << "void " << ns << name << "::openro(const std::string& filename, const char* vfs) {" << std::endl;
            of_src << "  db.openro(filename, vfs);" << std::endl;
            of_src << "  if(name.size() == 0){" << std::endl;
            of_src << "    name = filename;" << std::endl;
            of_src << "  }" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;
        } else {
            of_src << "void " << ns << name << "::open() {" << std::endl;
            for(auto& s : stmtList) {
                switch(s.action) {
                case SQLITE_INSERT:
                case SQLITE_UPDATE:
                case SQLITE_DELETE:
                case SQLITE_SELECT:
                    of_src << "  " << s.qname() << "_.open(" << s.qname() << "_s());" << std::endl;
                    break;
                }
            }
            of_src << "}" << std::endl;
            of_src << std::endl;
        }
        of_src << std::endl;
    }

    inline void generate(const std::string& odir, const Module& module) {
        auto bname = odir + module.name;
        std::cout << "Generating:[" << bname + ".hpp"
                  << "] and [" << bname + ".cpp"
                  << "]" << std::endl;

        // open src and hdr files
        std::ofstream of_hdr(bname + ".hpp");
        if(!of_hdr.is_open()) {
            std::cout << "Error:Unable to open file:" << bname << ".hpp" << std::endl;
            exit(1);
        }
        std::ofstream of_src(bname + ".cpp");
        if(!of_src.is_open()) {
            std::cout << "Error:Unable to open file:" << bname << ".cpp" << std::endl;
            exit(1);
        }

        // HDR:generate includes
        of_hdr << "#pragma once" << std::endl;
        of_hdr << std::endl;
        of_hdr << "#include <string>" << std::endl;
        of_hdr << "#include <vector>" << std::endl;
        of_hdr << "#include <sqlite3.h>" << std::endl;
        for(auto& i : module.includeList) {
            of_hdr << "#include \"" << i << "\"" << std::endl;
        }
        of_hdr << std::endl;

        // SRC:generate includes
        of_src << "#include <iostream>" << std::endl;
        for(auto& i : module.importList) {
            of_src << "#include \"" << i << "\"" << std::endl;
        }
        of_src << "#include \"" << module.name << ".hpp\"" << std::endl;
        of_src << std::endl;

        // HDR:generate declaration for common structs required by sqlch
        if(module.generateBase != false) {
            // SQLCH_COMMON used is to ensure that the structs are not redefined when more than one
            // of the generated files are included in the same cpp file
            of_hdr << "#if !defined(SQLCH_COMMON)" << std::endl;
            of_hdr << "#define SQLCH_COMMON 1" << std::endl;
            of_hdr << "namespace " << module.generateBaseNS << " {" << std::endl;
            of_hdr << "  std::string error(sqlite3* db);" << std::endl;
            of_hdr << "  struct database;" << std::endl;
            of_hdr << "  struct statement {" << std::endl;
            of_hdr << "    database& db_;" << std::endl;
            of_hdr << "    sqlite3_stmt* val_;" << std::endl;
            of_hdr << "    void open(const std::string& sql);" << std::endl;
            of_hdr << "    void close();" << std::endl;
            of_hdr << "    bool next();" << std::endl;
            if(module.isAutoIncrement){
                of_hdr << "    uint64_t insert();" << std::endl;
            }else{
                of_hdr << "    void insert();" << std::endl;
            }
            of_hdr << "    void xdelete();" << std::endl;
            of_hdr << "    void reset();" << std::endl;
            of_hdr << "    size_t getColumnCount();" << std::endl;
            of_hdr << "    int getColumnType(const size_t& idx);" << std::endl;
            of_hdr << "    void setParamFloat(const std::string& key, const double& val);" << std::endl;
            of_hdr << "    double getColumnFloat(const int& idx);" << std::endl;
            of_hdr << "    void setParamLong(const std::string& key, const int64_t& val);" << std::endl;
            of_hdr << "    int64_t getColumnLong(const int& idx);" << std::endl;
            of_hdr << "    void setParamText(const std::string& key, const std::string& val);" << std::endl;
            of_hdr << "    std::string getColumnText(const int& idx);" << std::endl;
            of_hdr << "    template <typename T> inline void setParam(const std::string& key, const T& val);" << std::endl;
            of_hdr << "    template <typename T> inline T getColumn(const int& idx);" << std::endl;
            of_hdr << "  protected:" << std::endl;
            of_hdr << "    inline statement(database& db) : db_(db), val_(nullptr){}" << std::endl;
            of_hdr << "    inline statement(const statement&) = delete;" << std::endl;
            of_hdr << "    inline statement(statement&&) = delete;" << std::endl;
            of_hdr << "    inline ~statement() {close();}" << std::endl;
            of_hdr << "  };" << std::endl;
            of_hdr << "  template <> inline void statement::setParam<double>(const std::string& key, const double& val) { return setParamFloat(key, val); }" << std::endl;
            of_hdr << "  template <> inline double statement::getColumn<double>(const int& idx) { return getColumnFloat(idx); }" << std::endl;
            of_hdr << "  template <> inline void statement::setParam<int64_t>(const std::string& key, const int64_t& val) { return setParamLong(key, val); }" << std::endl;
            of_hdr << "  template <> inline int64_t statement::getColumn<int64_t>(const int& idx) { return getColumnLong(idx); }" << std::endl;
            of_hdr << "  template <> inline void statement::setParam<std::string>(const std::string& key, const std::string& val) { return setParamText(key, val); }" << std::endl;
            of_hdr << "  template <> inline std::string statement::getColumn<std::string>(const int& idx) { return getColumnText(idx); }" << std::endl;
            of_hdr << std::endl;

            of_hdr << "  struct exstatement : public " << module.generateBaseNS << "::statement {" << std::endl;
            of_hdr << "    inline exstatement(database& db) : statement(db){}" << std::endl;
            of_hdr << "  };" << std::endl;
            of_hdr << std::endl;

            of_hdr << "  struct database {" << std::endl;
            of_hdr << "    sqlite3* val_;" << std::endl;
            of_hdr << "    exstatement beginTx_;" << std::endl;
            of_hdr << "    exstatement commitTx_;" << std::endl;
            of_hdr << "    std::string filename_;" << std::endl;
            //if(module.mutexName.length() > 0) {
            //    of_hdr << "#if " << module.mutexName << std::endl;
            //    of_hdr << "    std::mutex mx_;" << std::endl;
            //    of_hdr << "#endif //" << module.mutexName << std::endl;
            //}
            of_hdr << "    inline database(const database&) = delete;" << std::endl;
            of_hdr << "    inline database(database&&) = delete;" << std::endl;
            of_hdr << "    void open(const std::string& filename, const int& flags, const char* vfs);" << std::endl;
            of_hdr << "    void close();" << std::endl;
            of_hdr << "    inline void begin(){beginTx_.reset();beginTx_.next();}" << std::endl;
            of_hdr << "    inline void commit(){commitTx_.reset();commitTx_.next();}" << std::endl;
            of_hdr << "    void exec(const std::string& sqls);" << std::endl;
            of_hdr << "    inline void create(const std::string& filename, const char* vfs){open(filename, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, vfs);}" << std::endl;
            of_hdr << "    inline void openro(const std::string& filename, const char* vfs){open(filename, SQLITE_OPEN_READONLY, vfs);}" << std::endl;
            of_hdr << "    inline void openrw(const std::string& filename, const char* vfs){open(filename, SQLITE_OPEN_READWRITE, vfs);}" << std::endl;
            of_hdr << "    inline auto isOpen() {return (val_ != nullptr);}" << std::endl;
            of_hdr << "    inline auto& filename() const {return filename_;}" << std::endl;
            of_hdr << "    inline database() : val_(nullptr), beginTx_(*this), commitTx_(*this) {}" << std::endl;
            of_hdr << "    inline ~database() {close();}" << std::endl;
            of_hdr << "  };" << std::endl;
            of_hdr << std::endl;

            of_hdr << "  struct guard {" << std::endl;
            //if(module.mutexName.length() > 0) {
            //    of_hdr << "#if " << module.mutexName << std::endl;
            //    of_hdr << "    std::lock_guard<std::mutex> lk_;" << std::endl;
            //    of_hdr << "#endif //" << module.mutexName << std::endl;
            //    of_hdr << "#if " << module.mutexName << std::endl;
            //    of_hdr << "    inline guard(database& db) : lk_(db.mx_){}" << std::endl;
            //    of_hdr << "#else" << std::endl;
            //    of_hdr << "    inline guard(database&){}" << std::endl;
            //    of_hdr << "#endif //" << module.mutexName << std::endl;
            //} else {
            //    of_hdr << "    inline guard(database&){}" << std::endl;
            //}
            of_hdr << "    inline guard(database&){}" << std::endl;
            of_hdr << "  };" << std::endl;
            of_hdr << std::endl;

            //////
            of_hdr << "  template <typename DbT, typename ConnT>" << std::endl;
            of_hdr << "  class pool {" << std::endl;
            of_hdr << "      DbT& db_;" << std::endl;
            of_hdr << "      std::vector<std::unique_ptr<ConnT> > pool_;" << std::endl;
            of_hdr << "      std::vector<ConnT*> free_;" << std::endl;
            if (module.mutexName.length() > 0) {
                of_hdr << "#if " << module.mutexName << std::endl;
                of_hdr << "       std::mutex mx_;" << std::endl;
                of_hdr << "#endif //" << module.mutexName << std::endl;
            }
            of_hdr << "  public:" << std::endl;
            of_hdr << "      inline ConnT* get() {" << std::endl;
            if (module.mutexName.length() > 0) {
                of_hdr << "#if " << module.mutexName << std::endl;
                of_hdr << "          std::lock_guard<std::mutex> lg(mx_);" << std::endl;
                of_hdr << "#endif //" << module.mutexName << std::endl;
            }
            of_hdr << "          if (free_.size() > 0) {" << std::endl;
            of_hdr << "              auto r = free_.back();" << std::endl;
            of_hdr << "              free_.pop_back();" << std::endl;
            of_hdr << "              return r;" << std::endl;
            of_hdr << "          }" << std::endl;
            of_hdr << "          std::unique_ptr<ConnT> ro(new ConnT(db_));" << std::endl;
            of_hdr << "          ro->open();" << std::endl;
            of_hdr << "          pool_.push_back(std::move(ro));" << std::endl;
            of_hdr << "          return pool_.back().get();" << std::endl;
            of_hdr << "      }" << std::endl;

            of_hdr << "      inline void release(ConnT* r) {" << std::endl;
            if (module.mutexName.length() > 0) {
                of_hdr << "#if " << module.mutexName << std::endl;
                of_hdr << "          std::lock_guard<std::mutex> lg(mx_);" << std::endl;
                of_hdr << "#endif //" << module.mutexName << std::endl;
            }
            of_hdr << "          free_.push_back(r);" << std::endl;
            of_hdr << "      }" << std::endl;

            of_hdr << "      inline pool(DbT& db) : db_(db) {}" << std::endl;

            of_hdr << "      class guard {" << std::endl;
            of_hdr << "          pool& cp_;" << std::endl;
            of_hdr << "          ConnT* iface_;" << std::endl;
            of_hdr << "      public:" << std::endl;
            of_hdr << "          inline guard(pool& cp) : cp_(cp), iface_(nullptr) {" << std::endl;
            of_hdr << "              iface_ = cp_.get();" << std::endl;
            of_hdr << "          }" << std::endl;
            of_hdr << "          inline ~guard() {" << std::endl;
            of_hdr << "              cp_.release(iface_);" << std::endl;
            of_hdr << "          }" << std::endl;
            of_hdr << "          inline auto& conn() {" << std::endl;
            of_hdr << "              return *iface_;" << std::endl;
            of_hdr << "          }" << std::endl;
            of_hdr << "          inline operator bool() {" << std::endl;
            of_hdr << "              return (iface_ == nullptr);" << std::endl;
            of_hdr << "          }" << std::endl;
            of_hdr << "      };" << std::endl;
            of_hdr << "  };" << std::endl;
            //////

            of_hdr << "  struct transaction {" << std::endl;
            of_hdr << "    database& db_;" << std::endl;
            of_hdr << "    bool committed_;" << std::endl;
            of_hdr << "    inline void begin(){db_.begin();}" << std::endl;
            of_hdr << "    inline void commit(){db_.commit();committed_ = true;}" << std::endl;
            of_hdr << "    inline void rollback(){db_.exec(\"ROLLBACK;\");}" << std::endl;
            of_hdr << "    inline transaction& operator=(const transaction& src) = delete;" << std::endl;
            of_hdr << "    inline transaction(database& db) : db_(db), committed_(false) { begin(); }" << std::endl;
            of_hdr << "    inline ~transaction() { if (!committed_) rollback(); }" << std::endl;
            of_hdr << "    " << std::endl;
            of_hdr << "  };" << std::endl;
            of_hdr << std::endl;

            of_hdr << "  template <typename T, typename I> struct iterator_base {" << std::endl;
            of_hdr << "    T& stmt;" << std::endl;
            of_hdr << "    bool last;" << std::endl;
            //if(module.mutexName.length() > 0) {
            //    of_hdr << "#if " << module.mutexName << std::endl;
            //    of_hdr << "      std::unique_lock<std::mutex> lk;" << std::endl;
            //    of_hdr << "#endif //" << module.mutexName << std::endl;
            //}
            of_hdr << "    inline void next() {last = (stmt.next() == false);}" << std::endl;
            of_hdr << "    inline bool operator  =(const iterator_base& rhs) = delete;" << std::endl;
            of_hdr << "    inline bool operator !=(const iterator_base& rhs) {return last != rhs.last;}" << std::endl;
            of_hdr << "    inline bool operator ==(const iterator_base& rhs) {return last == rhs.last;}" << std::endl;
            of_hdr << "    inline I& operator++() {next();return static_cast<I&>(*this);}" << std::endl;
            of_hdr << "    inline I& operator*() {return static_cast<I&>(*this);}" << std::endl;

            of_hdr << "    inline iterator_base(T& s) : stmt(s), last(false) ";
            //if(module.mutexName.length() > 0) {
            //    of_hdr << std::endl;
            //    of_hdr << "#if " << module.mutexName << std::endl;
            //    of_hdr << "    , lk(s.db_.mx_)" << std::endl;
            //    of_hdr << "#endif //" << module.mutexName << std::endl;
            //    of_hdr << "    ";
            //}
            of_hdr << "{next();}" << std::endl;

            of_hdr << "    inline iterator_base(T& s, const bool& l) : stmt(s), last(l) " << std::endl;
            //if(module.mutexName.length() > 0) {
            //    of_hdr << std::endl;
            //    of_hdr << "#if " << module.mutexName << std::endl;
            //    of_hdr << "    , lk(s.db_.mx_, std::defer_lock)" << std::endl;
            //    of_hdr << "#endif //" << module.mutexName << std::endl;
            //    of_hdr << "    ";
            //}
            of_hdr << "{}" << std::endl;

            of_hdr << "    inline iterator_base(const iterator_base&) = delete;" << std::endl;
            of_hdr << "    inline iterator_base(iterator_base&& src) : stmt(src.stmt)";
            //if(module.mutexName.length() > 0) {
            //    of_hdr << std::endl;
            //    of_hdr << "#if " << module.mutexName << std::endl;
            //    of_hdr << "    , lk(stmt.db_.mx_)" << std::endl;
            //    of_hdr << "#endif //" << module.mutexName << std::endl;
            //    of_hdr << "    ";
            //}
            of_hdr << " {assert(false);}" << std::endl;
            of_hdr << "    inline ~iterator_base() {}" << std::endl;
            of_hdr << "  };" << std::endl;
            of_hdr << "}" << std::endl;
            of_hdr << std::endl;
            of_hdr << "#endif // !defined(SQLCH_COMMON)" << std::endl;
            of_hdr << std::endl;
        } else if(module.generateBase != false) {
            of_hdr << "#include \"" << module.generateBaseNS << "\"" << std::endl;
        }

        // HDR:open namespace in header file
        std::string ns;
        std::string nsx;
        if(module.nsList.size() > 0) {
            std::string nsep;
            for(auto& n : module.nsList) {
                of_hdr << "namespace " << n << " { ";
                nsx += nsep;
                nsx += n;
                nsep = "::";
            }
            ns = nsx + nsep; // appending terminating ::, if ns is defined
            of_hdr << std::endl;
        }

        // HDR: generate enum's
        for(auto& e : module.enumList) {
            of_hdr << "  enum class " << e.name << "{" << std::endl;
            std::string sep = " ";
            for(auto& v : e.valueList) {
                of_hdr << "   " << sep << v << std::endl;
                sep = ",";
            }
            of_hdr << "  };// enum" << e.name << std::endl;
            of_hdr << "  std::string to_string(const " << e.name << "& val);" << std::endl;
            of_hdr << "  inline std::ostream& operator<<(std::ostream& os, const " << e.name << "& val){" << std::endl;
            of_hdr << "    os << to_string(val);" << std::endl;
            of_hdr << "    return os;" << std::endl;
            of_hdr << "  }" << std::endl;
        }

        // SRC:generate definition for common structs required by sqlch
        // the ---SQLCH metacommand is used to ensure that it is defined only once
        // in the whole project
        if(module.generateBase != false) {
            // SRC: generate code
            of_src << module.scode << std::endl;
            of_src << std::endl;

            of_src << "namespace {" << std::endl;
            if(module.onError == "on_Error") {
                of_src << "  inline int on_Error(const std::string& db, const std::string& src, int rc, const std::string& msg){" << std::endl;
                of_src << "    std::cout << \"(\" << db << \"):sqlite error:\" << msg << \"(\" << rc << \") in \" << src << \", aborting.\" << std::endl;" << std::endl;
                of_src << "    exit(1);" << std::endl;
                of_src << "  }" << std::endl;
                of_src << std::endl;
            }
            if(module.onTrace != "") {
                of_src << "  void on_Trace(void* /*context*/, const char* /*sql*/){" << std::endl;
                of_src << "  }" << std::endl;
                of_src << std::endl;
            }
            if(module.onOpen == "on_Open") {
                of_src << "  inline void on_Open(const std::string& /*filename*/, const int& /*flags*/){" << std::endl;
                of_src << "  }" << std::endl;
                of_src << std::endl;
            }
            if(module.onOpened == "on_Opened") {
                of_src << "  inline void on_Opened(" << module.generateBaseNS << "::database& /*db*/){" << std::endl;
                of_src << "  }" << std::endl;
                of_src << std::endl;
            }

            of_src << "  inline int getParamIndex(" << module.generateBaseNS << "::statement& stmt, const std::string& key) {" << std::endl;
            of_src << "    if (stmt.val_ == nullptr) {" << std::endl;
            of_src << "      " << module.onError << "(stmt.db_.filename_, \"get_index\", SQLITE_MISUSE, \"uninitialized statement\");" << std::endl;
            of_src << "    }" << std::endl;
            of_src << "    int idx = ::sqlite3_bind_parameter_index(stmt.val_, key.c_str());" << std::endl;
            of_src << "    if (idx == 0) {" << std::endl;
            of_src << "      " << module.onError << "(stmt.db_.filename_, \"unknown_param\", SQLITE_MISUSE, key);" << std::endl;
            of_src << "    }" << std::endl;
            of_src << "    return idx;" << std::endl;
            of_src << "  }" << std::endl;
            of_src << "} // namespace" << std::endl;
            of_src << std::endl;

            of_src << "  std::string " << module.generateBaseNS << "::error(sqlite3* db){" << std::endl;
            of_src << "    return ::sqlite3_errmsg(db);" << std::endl;
            of_src << "  }" << std::endl;
            of_src << std::endl;

            of_src << "void " << module.generateBaseNS << "::database::open(const std::string& filename, const int& flags, const char* vfs){" << std::endl;
            if (module.mutexName.length() > 0) {
                of_src << "#if " << module.mutexName << std::endl;
                of_src << "  ::sqlite3_config(SQLITE_CONFIG_SERIALIZED);" << std::endl;
                of_src << "#else //" << module.mutexName << std::endl;
                of_src << "  ::sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);" << std::endl;
                of_src << "#endif //" << module.mutexName << std::endl;
            }
            of_src << "  if (val_ != nullptr) {close();}" << std::endl;
            of_src << "  " << module.onOpen << "(filename, flags);" << std::endl;
            of_src << "  int rc = ::sqlite3_open_v2(filename.c_str(), &val_, flags, vfs);" << std::endl;
            of_src << "  if (rc != SQLITE_OK) {" << std::endl;
            of_src << "    rc=" << module.onError << "(filename, \"open_db:\" + filename, rc, error(val_));" << std::endl;
            of_src << "    val_ = nullptr;" << std::endl;
            of_src << "    return;" << std::endl;
            of_src << "  }" << std::endl;

            if(module.onTrace != "") {
                of_src << "  sqlite3_trace(val_, &on_Trace, NULL);" << std::endl;
            }

            of_src << "  beginTx_.open(\"BEGIN EXCLUSIVE\");" << std::endl;
            of_src << "  commitTx_.open(\"COMMIT\");" << std::endl;

            of_src << "  ::sqlite3_busy_timeout(val_, 5000);" << std::endl;
            of_src << "  filename_ = filename;" << std::endl;
            of_src << "  " << module.onOpened << "(*this);" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "void " << module.generateBaseNS << "::database::close(){" << std::endl;
            of_src << "  if (val_) {" << std::endl;
            of_src << "    beginTx_.close();" << std::endl;
            of_src << "    commitTx_.close();" << std::endl;
            of_src << "    int rc = SQLITE_BUSY;" << std::endl;
            of_src << "    for (int i = 0; ((i < 10) && (rc == SQLITE_BUSY)); ++i) {" << std::endl;
            of_src << "      rc = ::sqlite3_close(val_);" << std::endl;
            of_src << "    }" << std::endl;
            of_src << "    if (rc > 0) {" << std::endl;
            of_src << "      " << module.onError << "(filename_, \"close_db\", rc, error(val_));" << std::endl;
            of_src << "    }" << std::endl;
            of_src << "  }" << std::endl;
            of_src << "  val_ = nullptr;" << std::endl;
            of_src << "  filename_ = \"\";" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "void " << module.generateBaseNS << "::database::exec(const std::string& sqls){" << std::endl;
            of_src << "  char* err = nullptr;" << std::endl;
            of_src << "  int rc = sqlite3_exec(val_, sqls.c_str(), nullptr, nullptr, &err);" << std::endl;
            of_src << "  if (rc != SQLITE_OK) {" << std::endl;
            of_src << "    std::string msg(err);" << std::endl;
            of_src << "    sqlite3_free(err);" << std::endl;
            of_src << "    " << module.onError << "(filename_, \"exec[\" + sqls + \"]\", rc, msg);" << std::endl;
            of_src << "  }" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "void " << module.generateBaseNS << "::statement::open(const std::string& sql){" << std::endl;
            of_src << "  if(db_.val_ == nullptr){" << std::endl;
            of_src << "    " << module.onError << "(db_.filename_, \"prepare\", SQLITE_MISUSE, \"[\" + sql + \"]:database not open\");" << std::endl;
            of_src << "    return;" << std::endl;
            of_src << "  }" << std::endl;
            of_src << "  int rc = ::sqlite3_prepare_v2(db_.val_, sql.c_str(), -1, &(val_), nullptr);" << std::endl;
            of_src << "  if(rc != SQLITE_OK){" << std::endl;
            of_src << "    " << module.onError << "(db_.filename_, \"prepare\", rc, \"[\" + sql + \"]:\" + error(db_.val_));" << std::endl;
            of_src << "    return;" << std::endl;
            of_src << "  }" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "void " << module.generateBaseNS << "::statement::close(){" << std::endl;
            of_src << "  if (val_) {" << std::endl;
            of_src << "    ::sqlite3_reset(val_);" << std::endl;
            of_src << "    ::sqlite3_clear_bindings(val_);" << std::endl;
            of_src << "    ::sqlite3_finalize(val_);" << std::endl;
            of_src << "  }" << std::endl;
            of_src << "  val_ = nullptr;" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "bool " << module.generateBaseNS << "::statement::next(){" << std::endl;
            of_src << "  int rc = ::sqlite3_step(val_);" << std::endl;
            of_src << "  if ((rc > 0) && (rc < 100)) {" << std::endl;
            of_src << "    rc=" << module.onError << "(db_.filename_, \"next\", rc, error(db_.val_));" << std::endl;
            of_src << "  }" << std::endl;
            of_src << "  return (rc == SQLITE_ROW);" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            if(module.isAutoIncrement){
                of_src << "uint64_t " << module.generateBaseNS << "::statement::insert(){" << std::endl;
                of_src << "  next();" << std::endl;
                of_src << "  return (uint64_t)::sqlite3_last_insert_rowid(db_.val_);" << std::endl;
                of_src << "}" << std::endl;
                of_src << std::endl;
            }else{
                of_src << "void " << module.generateBaseNS << "::statement::insert(){" << std::endl;
                of_src << "  next();" << std::endl;
                of_src << "}" << std::endl;
                of_src << std::endl;
            }

            of_src << "void " << module.generateBaseNS << "::statement::xdelete(){" << std::endl;
            of_src << "  next();" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "void " << module.generateBaseNS << "::statement::reset(){" << std::endl;
            of_src << "  int rc = ::sqlite3_reset(val_);" << std::endl;
            of_src << "  if (rc != SQLITE_OK) {" << std::endl;
            of_src << "    " << module.onError << "(db_.filename_, \"reset\", rc, error(db_.val_));" << std::endl;
            of_src << "  }" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "size_t " << module.generateBaseNS << "::statement::getColumnCount(){" << std::endl;
            of_src << "  return static_cast<size_t>(::sqlite3_column_count(val_));" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "int " << module.generateBaseNS << "::statement::getColumnType(const size_t& idx){" << std::endl;
            of_src << "  return ::sqlite3_column_type(val_, static_cast<int>(idx));" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "void " << module.generateBaseNS << "::statement::setParamFloat(const std::string& key, const double& val){" << std::endl;
            of_src << "  int idx = getParamIndex(*this, key);" << std::endl;
            of_src << "  ::sqlite3_bind_double(val_, idx, val);" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "double " << module.generateBaseNS << "::statement::getColumnFloat(const int& idx){" << std::endl;
            of_src << "  double val = ::sqlite3_column_double(val_, idx);" << std::endl;
            of_src << "  return val;" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "void " << module.generateBaseNS << "::statement::setParamLong(const std::string& key, const int64_t& val){" << std::endl;
            of_src << "  int idx = getParamIndex(*this, key);" << std::endl;
            of_src << "  ::sqlite3_bind_int64(val_, idx, val);" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "int64_t " << module.generateBaseNS << "::statement::getColumnLong(const int& idx){" << std::endl;
            of_src << "  int64_t val = ::sqlite3_column_int64(val_, idx);" << std::endl;
            of_src << "  return val;" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "void " << module.generateBaseNS << "::statement::setParamText(const std::string& key, const std::string& val){" << std::endl;
            of_src << "  int idx = getParamIndex(*this, key);" << std::endl;
            of_src << "#pragma clang diagnostic push" << std::endl;
            of_src << "#pragma clang diagnostic ignored \"-Wold-style-cast\"" << std::endl;
            of_src << "  ::sqlite3_bind_text(val_, idx, val.c_str(), static_cast<int>(val.length()), SQLITE_TRANSIENT);" << std::endl;
            of_src << "#pragma clang diagnostic pop" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;

            of_src << "std::string " << module.generateBaseNS << "::statement::getColumnText(const int& idx){" << std::endl;
            of_src << "  int len = ::sqlite3_column_bytes(val_, idx);" << std::endl;
            of_src << "  const void* valp = static_cast<const void*>(::sqlite3_column_text(val_, idx));" << std::endl;
            of_src << "  const char* val = static_cast<const char*>(valp);" << std::endl;
            of_src << "  if (val == nullptr) {" << std::endl;
            of_src << "    " << module.onError << "(db_.filename_, \"get_text\", SQLITE_ERROR, error(db_.val_));" << std::endl;
            of_src << "  }" << std::endl;
            of_src << "  return std::string(val, static_cast<size_t>(len));" << std::endl;
            of_src << "}" << std::endl;
        }
        of_src << std::endl;

        // SRC: generate enum's
        for(auto& e : module.enumList) {
            of_src << "std::string " << ns << "to_string(const " << e.name << "& val){" << std::endl;
            of_src << "  switch(val){" << std::endl;
            for(auto& v : e.valueList) {
                of_src << "    case " << ns << e.name << "::" << v << ": return \"" << v << "\";" << std::endl;
            }
            of_src << "  }" << std::endl;
            of_src << "  return \"<UNKNOWN-ENUM:" << e.name << ">:\" + std::to_string(static_cast<int>(val));" << std::endl;
            of_src << "}" << std::endl;
            of_src << std::endl;
        }


        // generate interface declaraions
        for(auto& db : module.dbList) {
            for(auto& iface : db.interfaceList) {
                iface.generateIfaceDecl(module, of_hdr, of_src, ns);
            }
        }

        // generate statements
        for(auto& db : module.dbList) {
            for(auto& iface : db.interfaceList) {
                iface.generate(module, of_hdr, of_src, ns);
            }
        }

        // HDR: generate decsql declaration
        if(module.decSql.size() > 0){
            of_hdr << "  std::string " << module.decSql << "(const std::string& sql);" << std::endl;
        }
        of_hdr << std::endl;

        // HDR:close namespace in header file
        if(module.nsList.size() > 0) {
            for(auto& n : module.nsList) {
                of_hdr << " }";
            }
            of_hdr << " /* namespace " << nsx << " */" << std::endl;
            of_hdr << std::endl;
        }

        // HDR: generate code
        of_hdr << module.hcode << std::endl;
        of_hdr << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::string odir;
    std::vector<std::string> al;
    for(int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if(a == "-d") {
            if(i == (argc - 1)) {
                std::cout << "Invalid out directory" << std::endl;
                return 1;
            }
            ++i;
            odir = argv[i];
            continue;
        }
        al.push_back(a);
    }

    if(al.size() < 1) {
        std::cout << "no input files" << std::endl;
        exit(1);
    }

    auto fname = al.back();
    std::cout << "processing:" << fname << std::endl;
    al.pop_back();

    // extract filename
    auto mname = fname;
    auto npos = mname.find_last_of('/');
    if(npos == std::string::npos) {
        npos = mname.find_last_of('\\');
    }
    if(npos != std::string::npos) {
        mname = mname.substr(npos + 1);
    }
    npos = mname.find_last_of('.');
    if(npos != std::string::npos) {
        mname = mname.substr(0, npos);
    }

    // process file
    Module module(mname);
    readFile(fname, module);
    generate(odir, module);

    return 0;
}
