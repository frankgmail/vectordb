#include <glog/logging.h>
#include "util.h"
#include "coding.h"
#include "meta.h"

namespace vectordb {

std::string
EngineTypeToString(EngineType e) {
    if (e == kVEngineAnnoy) {
        return "kVEngineAnnoy";

    } else if (e == kVEngineFaiss) {
        return "kVEngineFaiss";

    } else if (e == kGEngineEasyGraph) {
        return "kGEngineEasyGraph";

    }
    return "unknown engine";
}

Meta::Meta(const std::string &path)
    :path_(path) {
}

Status
Meta::Load() {
    bool b;
    leveldb::Status s;
    std::string value;
    std::vector<std::string> table_names;

    s = db_->Get(leveldb::ReadOptions(), META_PERSIST_KEY_TABLES, &value);
    if (s.IsNotFound()) {
        return Status::OK();
    }
    assert(s.ok());

    b = Str2TableNames(value, table_names);
    assert(b);

    for (auto &table_name : table_names) {
        LOG(INFO) << "loading meta... table:" << table_name;
        s = db_->Get(leveldb::ReadOptions(), table_name, &value);
        assert(s.ok());

        Table table;
        b = Str2Table(value, table);
        assert(b);

        tables_.insert(std::pair<std::string, std::shared_ptr<Table>>
                       (table_name, std::make_shared<Table>(table)));
    }

    LOG(INFO) << "meta load: " << ToStringShort();

    return Status::OK();
}

Status
Meta::Persist() {
    std::vector<std::string> table_names;
    for (auto &t : tables_) {
        table_names.push_back(t.first);
    }

    leveldb::Status s;
    leveldb::WriteOptions write_options;
    write_options.sync = true;
    std::string v;

    TableNames2Str(table_names, v);
    s = db_->Put(write_options, META_PERSIST_KEY_TABLES, v);
    assert(s.ok());

    for (auto &t : tables_) {
        std::string key = t.first;
        std::string value;
        Table &table = *(t.second);
        Table2Str(table, value);
        s = db_->Put(write_options, key, value);
        assert(s.ok());
    }

    return Status::OK();
}

Status
Meta::Init() {
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, path_, &db_);
    assert(status.ok());
    auto s = Load();
    assert(s.ok());
    return Status::OK();
}

Status
Meta::AddTable(const std::string &name,
               int partition_num,
               int replica_num,
               EngineType engine_type,
               const std::string &path) {
    auto it = tables_.find(name);
    if (it != tables_.end()) {
        std::string msg = name;
        msg.append(" already exist");
        return Status::Corruption(msg);
    }

    auto table = std::make_shared<Table>(name, partition_num, replica_num, engine_type, path);
    tables_.insert(std::pair<std::string, std::shared_ptr<Table>>(name, table));
    return Status::OK();
}

Status
Meta::DropTable(const std::string &name) {
    auto it = tables_.find(name);
    if (it == tables_.end()) {
        std::string msg = name;
        msg.append(" not exist");
        return Status::Corruption(msg);
    }

    tables_.erase(name);
    return Status::OK();
}

Status
Meta::ReplicaName(const std::string &table_name,
                  const std::string &key, std::string &replica_name) const {
    auto it_table = tables_.find(table_name);
    if (it_table == tables_.end()) {
        std::string msg;
        msg.append("table not found:");
        msg.append(table_name);
        return Status::NotFound(msg);
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "%s#partition0#replica0", table_name.c_str());
    replica_name = std::string(buf);
    return Status::OK();
}

}  // namespace vectordb
