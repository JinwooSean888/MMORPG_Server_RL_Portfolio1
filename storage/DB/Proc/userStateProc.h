#pragma once
#include <vector>
#include <string>
#include <mysql.h>
#include "storage/redis/redisUserCache.h"

namespace storage::sql {

    bool CallSpUpsertUserStateBatch(MYSQL* mysql,const std::vector<storage::redis::UserSnapshot>& snap);

} // namespace storage::sql
