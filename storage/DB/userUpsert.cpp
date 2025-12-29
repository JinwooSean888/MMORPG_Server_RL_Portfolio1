#include "storage/DB/userUpsert.h"
#include <mysql.h>
#include <cstring>

namespace storage::sql {

    bool UpsertUserState(MYSQL* mysql,
        const std::vector<storage::redis::UserSnapshot>& rows)
    {
        if (!mysql) return false;
        if (rows.empty()) return true;

        const char* q =
            "INSERT INTO user_state(uid, x, z, hp, sp, inv_json) "
            "VALUES(?,?,?,?,?,?) "
            "ON DUPLICATE KEY UPDATE "
            "x=VALUES(x), z=VALUES(z), hp=VALUES(hp), sp=VALUES(sp), inv_json=VALUES(inv_json)";

        MYSQL_STMT* stmt = mysql_stmt_init(mysql);
        if (!stmt) return false;

        if (mysql_stmt_prepare(stmt, q, (unsigned long)std::strlen(q)) != 0) {
            mysql_stmt_close(stmt);
            return false;
        }

        MYSQL_BIND bind[6];
        std::memset(bind, 0, sizeof(bind));

        unsigned long inv_len = 0;
        my_bool inv_is_null = 0;

        unsigned long long uid = 0;
        float x = 0.f, z = 0.f;
        int hp = 0, sp = 0;
        const char* inv_ptr = nullptr;

        // uid
        bind[0].buffer_type = MYSQL_TYPE_LONGLONG;
        bind[0].buffer = &uid;
        bind[0].is_unsigned = 1;

        // x, z
        bind[1].buffer_type = MYSQL_TYPE_FLOAT;
        bind[1].buffer = &x;

        bind[2].buffer_type = MYSQL_TYPE_FLOAT;
        bind[2].buffer = &z;

        // hp, sp
        bind[3].buffer_type = MYSQL_TYPE_LONG;
        bind[3].buffer = &hp;

        bind[4].buffer_type = MYSQL_TYPE_LONG;
        bind[4].buffer = &sp;

        // inv_json (TEXT/JSON)
        bind[5].buffer_type = MYSQL_TYPE_STRING;
        bind[5].buffer = (void*)inv_ptr;         // 실행 전에 매번 갱신
        bind[5].buffer_length = 0;
        bind[5].length = &inv_len;
        bind[5].is_null = &inv_is_null;

        if (mysql_stmt_bind_param(stmt, bind) != 0) {
            mysql_stmt_close(stmt);
            return false;
        }

        for (const auto& r : rows) {
            uid = (unsigned long long)r.uid;
            x = r.x; z = r.z;
            hp = r.hp; sp = r.sp;

            if (r.inv_json.empty()) {
                inv_is_null = 1;
                inv_len = 0;
                bind[5].buffer = nullptr;
                bind[5].buffer_length = 0;
            }
            else {
                inv_is_null = 0;
                inv_len = (unsigned long)r.inv_json.size();
                bind[5].buffer = (void*)r.inv_json.data();
                bind[5].buffer_length = inv_len;
            }

            // bind[5].buffer를 바꿨으니 다시 bind해주는 게 안전(드라이버에 따라 필요)
            if (mysql_stmt_bind_param(stmt, bind) != 0) { mysql_stmt_close(stmt); return false; }

            if (mysql_stmt_execute(stmt) != 0) {
                // 여기서 mysql_stmt_error(stmt) 로그 찍으면 디버깅 쉬움
                mysql_stmt_close(stmt);
                return false;
            }
        }

        mysql_stmt_close(stmt);
        return true;
    }

} // namespace storage::sql
