// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_database.h"

#include "construct_serialization.h"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace navycraft {
namespace {
class Statement final {
public:
    Statement(sqlite3 *database, const char *sql)
    {
        if (sqlite3_prepare_v2(database, sql, -1, &m_statement, nullptr) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(database));
    }

    ~Statement()
    {
        sqlite3_finalize(m_statement);
    }

    sqlite3_stmt *get() const noexcept { return m_statement; }

private:
    sqlite3_stmt *m_statement = nullptr;
};

void check(sqlite3 *database, int result)
{
    if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW)
        throw std::runtime_error(sqlite3_errmsg(database));
}

void bindText(sqlite3 *database, sqlite3_stmt *statement, int index,
    const std::string &value)
{
    check(database, sqlite3_bind_text(statement, index, value.data(),
        static_cast<int>(value.size()), SQLITE_TRANSIENT));
}

void bindBlob(sqlite3 *database, sqlite3_stmt *statement, int index,
    const std::vector<std::uint8_t> &value)
{
    const void *data = value.empty() ? nullptr : value.data();
    check(database, sqlite3_bind_blob64(statement, index, data,
        static_cast<sqlite3_uint64>(value.size()), SQLITE_TRANSIENT));
}

std::vector<std::uint8_t> readBlob(sqlite3_stmt *statement, int column)
{
    const auto *data = static_cast<const std::uint8_t *>(sqlite3_column_blob(statement, column));
    const int size = sqlite3_column_bytes(statement, column);
    if (size <= 0)
        return {};
    return std::vector<std::uint8_t>(data, data + size);
}

std::string readText(sqlite3_stmt *statement, int column)
{
    const auto *text = sqlite3_column_text(statement, column);
    const int size = sqlite3_column_bytes(statement, column);
    if (!text || size <= 0)
        return {};
    return std::string(reinterpret_cast<const char *>(text), static_cast<std::size_t>(size));
}

std::int64_t unixNow()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::vector<std::uint8_t> encodeStrings(const std::vector<std::string> &values)
{
    std::vector<std::uint8_t> result;
    auto append32 = [&result](std::uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8)
            result.push_back(static_cast<std::uint8_t>(value >> shift));
    };
    if (values.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("too many construct rollback drop strings");
    append32(static_cast<std::uint32_t>(values.size()));
    for (const auto &value : values) {
        if (value.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("construct rollback drop string is too large");
        append32(static_cast<std::uint32_t>(value.size()));
        result.insert(result.end(), value.begin(), value.end());
    }
    return result;
}

std::vector<std::string> decodeStrings(const std::vector<std::uint8_t> &bytes)
{
    std::size_t offset = 0;
    auto read32 = [&]() {
        if (offset > bytes.size() || bytes.size() - offset < 4)
            throw std::runtime_error("truncated construct rollback string list");
        std::uint32_t value = 0;
        for (unsigned shift = 0; shift < 32; shift += 8)
            value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
        return value;
    };
    const std::uint32_t count = read32();
    std::vector<std::string> result;
    result.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint32_t size = read32();
        if (offset > bytes.size() || size > bytes.size() - offset)
            throw std::runtime_error("truncated construct rollback string");
        result.emplace_back(reinterpret_cast<const char *>(bytes.data() + offset), size);
        offset += size;
    }
    if (offset != bytes.size())
        throw std::runtime_error("trailing construct rollback string bytes");
    return result;
}

std::optional<ConstructNode> nodeFromPayload(
    const std::vector<std::uint8_t> &payload, const LocalNodePos &position)
{
    if (payload.empty())
        return std::nullopt;
    const auto construct = ConstructSerialization::decode(payload);
    const auto *node = construct->getNode(position);
    return node ? std::optional<ConstructNode>(*node) : std::nullopt;
}
}

ConstructDatabase::ConstructDatabase(const std::string &path)
{
    if (path.empty())
        throw std::invalid_argument("construct database path cannot be empty");
    if (sqlite3_open_v2(path.c_str(), &m_database,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
            nullptr) != SQLITE_OK) {
        const std::string message = m_database ? sqlite3_errmsg(m_database) :
            "failed to allocate SQLite database";
        if (m_database)
            sqlite3_close(m_database);
        m_database = nullptr;
        throw std::runtime_error(message);
    }
    execute("PRAGMA journal_mode=WAL;");
    execute("PRAGMA synchronous=NORMAL;");
    execute("PRAGMA foreign_keys=ON;");
    initialiseSchema();
}

ConstructDatabase::~ConstructDatabase()
{
    if (m_database)
        sqlite3_close(m_database);
}

void ConstructDatabase::execute(const char *sql) const
{
    char *error = nullptr;
    const int result = sqlite3_exec(m_database, sql, nullptr, nullptr, &error);
    if (result != SQLITE_OK) {
        const std::string message = error ? error : sqlite3_errmsg(m_database);
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

void ConstructDatabase::initialiseSchema()
{
    execute("CREATE TABLE IF NOT EXISTS navycraft_meta("
        "key TEXT PRIMARY KEY,value TEXT NOT NULL);");
    execute("INSERT OR REPLACE INTO navycraft_meta(key,value) "
        "VALUES('schema_version','1');");
    execute("CREATE TABLE IF NOT EXISTS navycraft_constructs("
        "construct_id INTEGER PRIMARY KEY,"
        "node_revision INTEGER NOT NULL,"
        "payload BLOB NOT NULL,"
        "updated_at INTEGER NOT NULL);");
    execute("CREATE TABLE IF NOT EXISTS navycraft_actions("
        "action_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "construct_id INTEGER NOT NULL,"
        "actor TEXT NOT NULL,"
        "action INTEGER NOT NULL,"
        "local_x INTEGER NOT NULL,local_y INTEGER NOT NULL,local_z INTEGER NOT NULL,"
        "accepted INTEGER NOT NULL,protected_violation INTEGER NOT NULL,"
        "item_before TEXT NOT NULL,item_after TEXT NOT NULL,drops BLOB NOT NULL,"
        "reason TEXT NOT NULL,before_payload BLOB NOT NULL,after_payload BLOB NOT NULL,"
        "created_at INTEGER NOT NULL);");
    execute("CREATE INDEX IF NOT EXISTS navycraft_actions_construct_time "
        "ON navycraft_actions(construct_id,action_id DESC);");
}

void ConstructDatabase::saveConstruct(const DynamicConstruct &construct)
{
    const auto payload = ConstructSerialization::encode(construct);
    Statement statement(m_database,
        "INSERT INTO navycraft_constructs(construct_id,node_revision,payload,updated_at) "
        "VALUES(?,?,?,?) ON CONFLICT(construct_id) DO UPDATE SET "
        "node_revision=excluded.node_revision,payload=excluded.payload,"
        "updated_at=excluded.updated_at;");
    check(m_database, sqlite3_bind_int64(statement.get(), 1,
        static_cast<sqlite3_int64>(construct.id())));
    check(m_database, sqlite3_bind_int64(statement.get(), 2,
        static_cast<sqlite3_int64>(construct.nodeRevision())));
    bindBlob(m_database, statement.get(), 3, payload);
    check(m_database, sqlite3_bind_int64(statement.get(), 4, unixNow()));
    check(m_database, sqlite3_step(statement.get()));
}

std::shared_ptr<DynamicConstruct> ConstructDatabase::loadConstruct(ConstructId id) const
{
    Statement statement(m_database,
        "SELECT payload FROM navycraft_constructs WHERE construct_id=?;");
    check(m_database, sqlite3_bind_int64(statement.get(), 1,
        static_cast<sqlite3_int64>(id)));
    const int result = sqlite3_step(statement.get());
    if (result == SQLITE_DONE)
        return nullptr;
    check(m_database, result);
    return ConstructSerialization::decode(readBlob(statement.get(), 0));
}

std::vector<std::shared_ptr<DynamicConstruct>> ConstructDatabase::loadAllConstructs() const
{
    Statement statement(m_database,
        "SELECT payload FROM navycraft_constructs ORDER BY construct_id;");
    std::vector<std::shared_ptr<DynamicConstruct>> result;
    for (;;) {
        const int step = sqlite3_step(statement.get());
        if (step == SQLITE_DONE)
            break;
        check(m_database, step);
        result.push_back(ConstructSerialization::decode(readBlob(statement.get(), 0)));
    }
    return result;
}

bool ConstructDatabase::deleteConstruct(ConstructId id)
{
    Statement statement(m_database,
        "DELETE FROM navycraft_constructs WHERE construct_id=?;");
    check(m_database, sqlite3_bind_int64(statement.get(), 1,
        static_cast<sqlite3_int64>(id)));
    check(m_database, sqlite3_step(statement.get()));
    return sqlite3_changes(m_database) != 0;
}

void ConstructDatabase::syncConstructs(
    const std::vector<std::shared_ptr<DynamicConstruct>> &constructs)
{
    execute("BEGIN IMMEDIATE;");
    try {
        std::unordered_set<ConstructId> retained;
        retained.reserve(constructs.size());
        for (const auto &construct : constructs) {
            if (!construct)
                continue;
            saveConstruct(*construct);
            retained.insert(construct->id());
        }
        Statement list(m_database, "SELECT construct_id FROM navycraft_constructs;");
        std::vector<ConstructId> stale;
        for (;;) {
            const int step = sqlite3_step(list.get());
            if (step == SQLITE_DONE)
                break;
            check(m_database, step);
            const auto id = static_cast<ConstructId>(sqlite3_column_int64(list.get(), 0));
            if (retained.find(id) == retained.end())
                stale.push_back(id);
        }
        for (const auto id : stale)
            (void)deleteConstruct(id);
        execute("COMMIT;");
    } catch (...) {
        try { execute("ROLLBACK;"); } catch (...) {}
        throw;
    }
}

std::int64_t ConstructDatabase::recordMutation(
    const ConstructMutationRecord &mutation,
    const std::vector<std::uint8_t> &before_payload,
    const std::vector<std::uint8_t> &after_payload)
{
    const auto drops = encodeStrings(mutation.drops);
    Statement statement(m_database,
        "INSERT INTO navycraft_actions(construct_id,actor,action,local_x,local_y,local_z,"
        "accepted,protected_violation,item_before,item_after,drops,reason,"
        "before_payload,after_payload,created_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
    check(m_database, sqlite3_bind_int64(statement.get(), 1,
        static_cast<sqlite3_int64>(mutation.construct_id)));
    bindText(m_database, statement.get(), 2, mutation.actor);
    check(m_database, sqlite3_bind_int(statement.get(), 3,
        static_cast<int>(mutation.action)));
    check(m_database, sqlite3_bind_int(statement.get(), 4, mutation.position.x));
    check(m_database, sqlite3_bind_int(statement.get(), 5, mutation.position.y));
    check(m_database, sqlite3_bind_int(statement.get(), 6, mutation.position.z));
    check(m_database, sqlite3_bind_int(statement.get(), 7, mutation.accepted ? 1 : 0));
    check(m_database, sqlite3_bind_int(statement.get(), 8,
        mutation.protected_violation ? 1 : 0));
    bindText(m_database, statement.get(), 9, mutation.wielded_item_before);
    bindText(m_database, statement.get(), 10, mutation.wielded_item_after);
    bindBlob(m_database, statement.get(), 11, drops);
    bindText(m_database, statement.get(), 12, mutation.reason);
    bindBlob(m_database, statement.get(), 13, before_payload);
    bindBlob(m_database, statement.get(), 14, after_payload);
    check(m_database, sqlite3_bind_int64(statement.get(), 15, unixNow()));
    check(m_database, sqlite3_step(statement.get()));
    return sqlite3_last_insert_rowid(m_database);
}

std::vector<ConstructDatabaseAction> ConstructDatabase::recentActions(
    ConstructId construct_id, std::size_t limit) const
{
    limit = std::min<std::size_t>(limit, 10000);
    Statement statement(m_database,
        "SELECT action_id,actor,action,local_x,local_y,local_z,accepted,"
        "protected_violation,item_before,item_after,drops,reason,before_payload,"
        "after_payload,created_at FROM navycraft_actions WHERE construct_id=? "
        "ORDER BY action_id DESC LIMIT ?;");
    check(m_database, sqlite3_bind_int64(statement.get(), 1,
        static_cast<sqlite3_int64>(construct_id)));
    check(m_database, sqlite3_bind_int64(statement.get(), 2,
        static_cast<sqlite3_int64>(limit)));
    std::vector<ConstructDatabaseAction> result;
    for (;;) {
        const int step = sqlite3_step(statement.get());
        if (step == SQLITE_DONE)
            break;
        check(m_database, step);
        ConstructDatabaseAction action;
        action.action_id = sqlite3_column_int64(statement.get(), 0);
        action.mutation.construct_id = construct_id;
        action.mutation.actor = readText(statement.get(), 1);
        action.mutation.action = static_cast<ConstructInteractionAction>(
            sqlite3_column_int(statement.get(), 2));
        action.mutation.position = {sqlite3_column_int(statement.get(), 3),
            sqlite3_column_int(statement.get(), 4),
            sqlite3_column_int(statement.get(), 5)};
        action.mutation.accepted = sqlite3_column_int(statement.get(), 6) != 0;
        action.mutation.protected_violation = sqlite3_column_int(statement.get(), 7) != 0;
        action.mutation.wielded_item_before = readText(statement.get(), 8);
        action.mutation.wielded_item_after = readText(statement.get(), 9);
        action.mutation.drops = decodeStrings(readBlob(statement.get(), 10));
        action.mutation.reason = readText(statement.get(), 11);
        action.before_payload = readBlob(statement.get(), 12);
        action.after_payload = readBlob(statement.get(), 13);
        action.created_at = sqlite3_column_int64(statement.get(), 14);
        action.mutation.before_node = nodeFromPayload(
            action.before_payload, action.mutation.position);
        action.mutation.after_node = nodeFromPayload(
            action.after_payload, action.mutation.position);
        result.push_back(std::move(action));
    }
    return result;
}

std::shared_ptr<DynamicConstruct> ConstructDatabase::rollbackPayload(
    std::int64_t action_id) const
{
    Statement statement(m_database,
        "SELECT before_payload FROM navycraft_actions WHERE action_id=?;");
    check(m_database, sqlite3_bind_int64(statement.get(), 1, action_id));
    const int step = sqlite3_step(statement.get());
    if (step == SQLITE_DONE)
        return nullptr;
    check(m_database, step);
    const auto payload = readBlob(statement.get(), 0);
    return payload.empty() ? nullptr : ConstructSerialization::decode(payload);
}

std::size_t ConstructDatabase::constructCount() const
{
    Statement statement(m_database, "SELECT COUNT(*) FROM navycraft_constructs;");
    check(m_database, sqlite3_step(statement.get()));
    return static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 0));
}

std::size_t ConstructDatabase::actionCount() const
{
    Statement statement(m_database, "SELECT COUNT(*) FROM navycraft_actions;");
    check(m_database, sqlite3_step(statement.get()));
    return static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 0));
}

void ConstructDatabase::flush()
{
    check(m_database, sqlite3_wal_checkpoint_v2(
        m_database, nullptr, SQLITE_CHECKPOINT_PASSIVE, nullptr, nullptr));
}

} // namespace navycraft
