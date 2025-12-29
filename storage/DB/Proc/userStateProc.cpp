#include "storage/DB/Proc/userStateProc.h"
#include <sstream>
#include <iomanip>

namespace storage::sql {

    static std::string BuildSnapshotJson(const std::vector<storage::redis::UserSnapshot>& snap)
    {
        std::ostringstream oss;
        oss << '[';
        for (size_t i = 0; i < snap.size(); ++i) {
            const auto& s = snap[i];
            if (i) oss << ',';
            oss << '{'
                << "\"uid\":" << s.uid << ','
                << "\"x\":" << std::setprecision(9) << (double)s.x << ','
                << "\"z\":" << std::setprecision(9) << (double)s.z << ','
                << "\"hp\":" << s.hp << ','
                << "\"sp\":" << s.sp
                << '}';
        }
        oss << ']';
        return oss.str();
    }

    bool CallSpUpsertUserStateBatch(MYSQL* mysql,
        const std::vector<storage::redis::UserSnapshot>& snap)
    {
        if (!mysql) return false;
        if (snap.empty()) return true;

        const std::string json = BuildSnapshotJson(snap);

        MYSQL_STMT* stmt = mysql_stmt_init(mysql);
        if (!stmt) return false;

        const char* sql = "CALL sp_upsert_user_state_batch(?)";
        if (mysql_stmt_prepare(stmt, sql, (unsigned long)std::strlen(sql)) != 0) {
            mysql_stmt_close(stmt);
            return false;
        }

        MYSQL_BIND bind[1]{};
        unsigned long json_len = (unsigned long)json.size();

        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (void*)json.data(); 
        bind[0].buffer_length = json_len;
        bind[0].length = &json_len;
        bind[0].is_null = 0;

        if (mysql_stmt_bind_param(stmt, bind) != 0) {
            mysql_stmt_close(stmt);
            return false;
        }

        if (mysql_stmt_execute(stmt) != 0) {
            mysql_stmt_close(stmt);
            return false;
        }

        while (mysql_stmt_next_result(stmt) == 0) {

        }

        mysql_stmt_close(stmt);
        return true;
    }

} // namespace storage::sql
