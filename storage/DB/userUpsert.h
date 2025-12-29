#pragma once
#include <vector>
#include <mysql.h>  //여기서 MYSQL 타입을 확정

#include "storage/redis/redisUserCache.h"

namespace storage::sql {

    bool UpsertUserState(MYSQL* mysql,
        const std::vector<storage::redis::UserSnapshot>& rows);

} // namespace storage::sql
