/************************************************************************
Copyright 2021-2022 eBay Inc.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**************************************************************************/

#include "KVTransCommand.h"

#include <absl/strings/str_cat.h>
#include <rocksdb/db.h>
#include <spdlog/spdlog.h>

#include "../store/TemporalKVStore.h"
#include "../utils/StrUtil.h"

namespace goblin::kvengine::model {

KVTransCommandContext::KVTransCommandContext(const proto::Transaction::Request& req,
    AsyncTransCBFunc cb) : CommandContext(kType), mRequest(req), mCB(cb) {
}

void KVTransCommandContext::initSuccessResponse(const store::VersionType &curMaxVersion,
    const model::EventList &events) {
  mResponse.mutable_header()->set_code(proto::ResponseCode::OK);
  std::map<store::KeyType, store::VersionType> newKey2version;
  auto latestVersion = curMaxVersion;
  uint32_t i = 0;
  if (!events.empty()) {
    assert(events.size() == mRequest.entries().size());
    for (auto readOrWrite : mRequest.entries()) {
      auto &e = events[i++];
      auto newResult = mResponse.add_results();
      if (readOrWrite.has_writeentry()) {
        auto *write = dynamic_cast<const model::WriteEvent*>(e.get());
        assert(write != nullptr);
        auto allocatedVersion = write->getAllocatedVersion();
        newKey2version[write->getKey()] = allocatedVersion;
        newResult->mutable_writeresult()->set_version(allocatedVersion);
        newResult->mutable_writeresult()->set_code(proto::ResponseCode::OK);
        latestVersion = std::max(allocatedVersion, latestVersion);
      } else if (readOrWrite.has_readentry()) {
        auto *read = dynamic_cast<const model::ReadEvent*>(e.get());
        assert(read != nullptr);
        auto key = read->getKey();
        store::VersionType valueVersion = store::VersionStore::kInvalidVersion;
        if (newKey2version.find(key) != newKey2version.end()) {
          /// we are reading the same key we newly inserted in this transaction
          valueVersion = newKey2version[key];
        } else {
          valueVersion = read->getValueVersion();
        }
        newResult->mutable_readresult()->set_value(read->getValue());
        newResult->mutable_readresult()->set_version(valueVersion);
        if (read->isNotFound()) {
          newResult->mutable_readresult()->set_code(proto::ResponseCode::KEY_NOT_EXIST);
          newResult->mutable_readresult()->set_version(store::VersionStore::kInvalidVersion);
        } else {
          newResult->mutable_readresult()->set_code(proto::ResponseCode::OK);
          if (mRequest.ismetareturned()) {
            newResult->mutable_readresult()->set_updatetime(read->getUpdateTime());
            auto &udfMeta = read->getUdfMeta();
            for (auto val : udfMeta.uintfield()) {
              newResult->mutable_readresult()->mutable_udfmeta()->add_uintfield(val);
            }
            for (auto str : udfMeta.strfield()) {
              newResult->mutable_readresult()->mutable_udfmeta()->add_strfield(str);
            }
          }
        }
        latestVersion = std::max(valueVersion, latestVersion);
      } else if (readOrWrite.has_removeentry()) {
        store::VersionType valueVersion = store::VersionStore::kInvalidVersion;
        if (e->getType() == model::EventType::READ) {
          auto *read = dynamic_cast<const model::ReadEvent*>(e.get());
          assert(read->isNotFound());
          newResult->mutable_removeresult()->set_code(proto::ResponseCode::KEY_NOT_EXIST);
          newResult->mutable_removeresult()->set_version(store::VersionStore::kInvalidVersion);
          valueVersion = read->getValueVersion();
        } else {
          auto *deleteEvent = dynamic_cast<const model::DeleteEvent*>(e.get());
          assert(deleteEvent != nullptr);
          newResult->mutable_removeresult()->set_value(deleteEvent->getDeletedValue());
          newResult->mutable_removeresult()->set_version(deleteEvent->getDeletedVersion());
          newResult->mutable_removeresult()->set_code(proto::ResponseCode::OK);
          valueVersion = deleteEvent->getAllocatedVersion();
        }
        latestVersion = std::max(valueVersion, latestVersion);
      } else {
        assert(0);
      }
    }
  }
  mResponse.mutable_header()->set_latestversion(latestVersion);
  /// return ignored keys in usermetacond
  for (auto &key : ignoredPutKeys) {
    mResponse.add_ignoredputkeys(key);
  }
}

void KVTransCommandContext::fillResponseAndReply(proto::ResponseCode code,
    const std::string &message, std::optional<uint64_t> leaderId) {
  if (code == proto::ResponseCode::NOT_LEADER && leaderId) {
    mResponse.mutable_header()->set_leaderhint(std::to_string(*leaderId));
    mResponse.mutable_header()->set_leaderip(goblin::kvengine::utils::ClusterInfoUtil::getIpFromNodeId(leaderId));
  } else if (code == proto::ResponseCode::WRONG_ROUTE) {
    std::vector<std::string> addrs = utils::StrUtil::tokenize(message, ',');
    assert(!addrs.empty());
    for (auto addr : addrs) {
      auto server = mResponse.mutable_header()->mutable_routehint()->add_servers();
      server->set_hostname(addr);
    }
  }
  mResponse.mutable_header()->set_code(code);
  if (code != proto::ResponseCode::OK) {
    for (auto &r : *mResponse.mutable_results()) {
      if (r.has_writeresult()) {
        r.mutable_writeresult()->set_code(code);
      } else if (r.has_readresult()) {
        r.mutable_readresult()->set_code(code);
      } else if (r.has_removeresult()) {
        r.mutable_removeresult()->set_code(code);
      } else {
        assert(0);
      }
    }
  }
  mResponse.mutable_header()->set_message(message);
  mCB(mResponse);
}

void KVTransCommandContext::reportSubMetrics() {
  /// metrics counter
  auto transCounter = gringofts::getCounter("trans_command_counter", {});
  transCounter.increase();
  /// SPDLOG_INFO("debug: trans op");
}

void KVTransCommandContext::addIgnoredPutkey(const store::KeyType &key) {
  ignoredPutKeys.insert(key);
}

const proto::Transaction::Request& KVTransCommandContext::getRequest() {
  return mRequest;
}

KVTransCommand::KVTransCommand(std::shared_ptr<KVTransCommandContext> context):
  mContext(context) {
}

std::shared_ptr<CommandContext> KVTransCommand::getContext() {
  return mContext;
}

bool KVTransCommand::isFollowerReadCmd() const {
  // auto context = getContext();
  auto &req = mContext->getRequest();
  if (!req.allowstale()) {
    // not enable read from follower
    return false;
  }
  for (const auto &readOrWrite : req.entries()) {
    if (readOrWrite.has_readentry()) {
      continue;
    } else if (readOrWrite.has_writeentry()) {
      SPDLOG_WARN("trans has write entry, cannot do it in follower");
      return false;
    } else if (readOrWrite.has_removeentry()) {
      SPDLOG_WARN("trans has remove entry, cannot do it in follower");
      return false;
    } else {
      SPDLOG_WARN("trans has unknown entry, cannot do it in follower");
      return false;
    }
  }
  return true;
}

std::set<store::KeyType> KVTransCommandContext::getTargetKeys() {
  std::set<store::KeyType> keys;
  for (auto &cond : mRequest.preconds()) {
    if (cond.has_versioncond()) {
      keys.insert(cond.versioncond().key());
    } else if (cond.has_existcond()) {
      keys.insert(cond.existcond().key());
    } else if (cond.has_usermetacond()) {
      keys.insert(cond.usermetacond().key());
    }
  }
  for (auto readOrWrite : mRequest.entries()) {
    if (readOrWrite.has_writeentry()) {
      const store::KeyType &key = readOrWrite.writeentry().key();
      keys.insert(key);
    } else if (readOrWrite.has_readentry()) {
      const store::KeyType &key = readOrWrite.readentry().key();
      keys.insert(key);
    } else if (readOrWrite.has_removeentry()) {
      const store::KeyType &key = readOrWrite.removeentry().key();
      keys.insert(key);
    } else {
      assert(0);
    }
  }
  return keys;
}

proto::RequestHeader KVTransCommandContext::getRequestHeader() {
  return mRequest.header();
}

utils::Status KVTransCommand::prepare(const std::shared_ptr<store::KVStore> &kvStore) {
  /// acquire exclusive lock
  return kvStore->lock(mContext->getTargetKeys(), true);
}

utils::Status KVTransCommand::checkPrecond(std::shared_ptr<store::KVStore> kvStore) {
  auto &req = mContext->getRequest();
  utils::Status s = utils::Status::ok();
  std::unordered_set<store::KeyType> udfKeySet;
  bool isFollowerReadCmd = this->isFollowerReadCmd();
  for (auto &cond : req.preconds()) {
    if (cond.has_versioncond()) {
      auto key = cond.versioncond().key();
      auto expectedVersion = cond.versioncond().version();
      auto op = cond.versioncond().op();
      store::ValueType value;
      store::TTLType ttl = store::INFINITE_TTL;
      store::VersionType version = store::VersionStore::kInvalidVersion;
      s = this->readKV(kvStore, isFollowerReadCmd, key, &value, &ttl, &version);
      SPDLOG_DEBUG("debug: precond, key {}, value size {}, newVersion {}", key, value.size(), version);
      if (!s.isOK()) {
        s = utils::Status::precondUnmatched(s.getDetail());
        SPDLOG_WARN("check failed {}", s.getDetail());
        break;
      }
      if (op == proto::Precondition::EQUAL) {
        if (expectedVersion != version) {
          s = utils::Status::precondUnmatched(
              absl::StrCat("unmatched version, existing: ",
                std::to_string(version), ", expected: ",
                std::to_string(expectedVersion)));
          SPDLOG_WARN("check failed {}", s.getDetail());
          break;
        }
      } else {
        s = utils::Status::notSupported();
        break;
      }
    } else if (cond.has_existcond()) {
      auto key = cond.existcond().key();
      auto shouldExist = cond.existcond().shouldexist();
      store::ValueType value;
      store::TTLType ttl;
      store::VersionType version;
      s = this->readKV(kvStore, isFollowerReadCmd, key, &value, &ttl, &version);
      SPDLOG_DEBUG("debug: shouldexist {}, key {}, result {}", shouldExist, key, s.getDetail());
      if (shouldExist && s.isOK()) {
        s = utils::Status::ok();
      } else if (!shouldExist && s.isNotFound()) {
        s = utils::Status::ok();
      } else {
        s = utils::Status::precondUnmatched(
            absl::StrCat("shouldExist: ",
              std::to_string(shouldExist), ", query result: ",
              s.getDetail()));
        break;
      }
    } else if (cond.has_usermetacond()) {
      auto meta_type = cond.usermetacond().metatype();
      auto op = cond.usermetacond().op();
      auto key = cond.usermetacond().key();
      if ( udfKeySet.count(key) ) {
          s = utils::Status::notSupported();
          break;
      }
      udfKeySet.insert(key);

      store::ValueType value;
      store::TTLType ttl = store::INFINITE_TTL;
      store::VersionType version = store::VersionStore::kInvalidVersion;
      s = this->readKV(kvStore, isFollowerReadCmd, key, &value, &ttl, &version);
      SPDLOG_DEBUG("debug: usermetacond, key {}, isFound {}", key, !s.isNotFound());
      if (s.isNotFound()) {
        s = utils::Status::ok();
        continue;
      }
      goblin::proto::Meta meta;
      s = this->readMeta(kvStore, isFollowerReadCmd, key, &meta);

      if ( s.isNotFound() || !meta.has_udfmeta() ) {
        SPDLOG_DEBUG("debug: usermetacond, key {}, meta isFound {}, udfMeta isFound {}",
            key, !s.isNotFound(), meta.has_udfmeta());
        s = utils::Status::ok();
        continue;
      }
      auto cmp_pos = cond.usermetacond().pos();
      SPDLOG_DEBUG("debug: usermetacond, key {}, pos for usermetacond {}", key, cmp_pos);
      if (proto::Precondition::UserMetaCondition::NUM == meta_type) {
        auto &expected_uint_field = meta.udfmeta().uintfield();
        auto &provided_uint_field = cond.usermetacond().udfmeta().uintfield();
        if (expected_uint_field.empty()) {
          if (cmp_pos < 0 || cmp_pos >= provided_uint_field.size()) {
            s = utils::Status::error("out of index error");
            break;
          } else {
            SPDLOG_DEBUG("debug: usermetacond, key {}, uintfield not exists in stored udfmeta", key);
            s = utils::Status::ok();
          }
        } else if (cmp_pos < 0 || cmp_pos >= expected_uint_field.size() || cmp_pos >= provided_uint_field.size()) {
          s = utils::Status::error("out of index error");
          break;
        }
        auto expected_cmp_uint = expected_uint_field[cmp_pos];
        auto provided_cmp_uint = provided_uint_field[cmp_pos];
        if (op == proto::Precondition::GREATER) {
          if (provided_cmp_uint <= expected_cmp_uint) {
            ignoredUdfUintKeys.insert(key);
            mContext->addIgnoredPutkey(key);
            SPDLOG_DEBUG("debug: usermetacond, provided uint {} <= expected uint {}, key {} ignored",
                provided_cmp_uint, expected_cmp_uint, key);
          } else {
            SPDLOG_DEBUG("debug: usermetacond, provided uint {} > expected uint {}, key {}, value {}",
                provided_cmp_uint, expected_cmp_uint, key, value);
            /// save udfmeta
            udfUintKey2Meta[key] = cond.usermetacond().udfmeta();
          }
        } else {
          s = utils::Status::notSupported();
          break;
        }
      } else if (proto::Precondition::UserMetaCondition::STR == meta_type) {
        // To be implemented
        s = utils::Status::notSupported();
        break;
      } else {
        s = utils::Status::notSupported();
        break;
      }
    } else {
      s = utils::Status::notSupported();
      break;
    }
  }
  return s;
}

utils::Status KVTransCommand::execute(const std::shared_ptr<store::KVStore> &kvStore, EventList *events) {
  store::TemporalKVStore tempStore(std::make_shared<store::ReadOnlyKVStore>(kvStore));
  auto s = checkPrecond(kvStore);
  auto &req = mContext->getRequest();
  bool isFollowerReadCmd = this->isFollowerReadCmd();
  do {
    if (!s.isOK()) {
      break;
    }
    auto curTime = utils::TimeUtil::secondsSinceEpoch();
    for (auto readOrWrite : req.entries()) {
      if (readOrWrite.has_writeentry()) {
        const store::KeyType &key = readOrWrite.writeentry().key();
        const store::ValueType &value = readOrWrite.writeentry().value();
        bool enableTTL = readOrWrite.writeentry().enablettl();
        store::TTLType ttl = enableTTL? readOrWrite.writeentry().ttl() : store::INFINITE_TTL;

        if (ignoredUdfUintKeys.count(key)) {
          SPDLOG_DEBUG("debug: ignore put cmd, key {}, value size {}", key, value.size());
          events->push_back(std::make_shared<WriteEvent>(key, value, enableTTL, ttl, store::NEVER_EXPIRE,
            store::NOT_UPDATED, store::DEFAULT_UDFMETA, true));
          continue;
        }

        /// fix ttl value if not set while ttl enabled
        if (enableTTL && ttl == store::INFINITE_TTL) {
          proto::Meta meta;
          s = kvStore->readMeta(key, &meta);
          if (s.isOK()) {
            if (meta.ttl() != store::INFINITE_TTL && meta.deadline() > utils::TimeUtil::secondsSinceEpoch()) {
              ttl = meta.ttl();
            } else {
              enableTTL = false;
            }
          } else if (s.isNotFound()) {
            /// Keep the value forever if previous ttl not found or value is expired
            enableTTL = false;
          } else {
            break;
          }
        }
        const goblin::proto::UserDefinedMeta &udfMeta = readOrWrite.writeentry().udfmeta();
        const goblin::proto::UserDefinedMeta &condUdfMeta = udfUintKey2Meta.count(key)?
            udfUintKey2Meta[key] : store::DEFAULT_UDFMETA;
        if ( udfUintKey2Meta.count(key) ) {
          /// compare putUdfMeta and condUdfMeta
          /// return 400 if not identical
          bool uintCmpResult = std::equal(udfMeta.uintfield().begin(), udfMeta.uintfield().end(),
              condUdfMeta.uintfield().begin());
          bool strCmpResult = std::equal(udfMeta.strfield().begin(), udfMeta.strfield().end(),
              condUdfMeta.strfield().begin());
          if (!uintCmpResult || !strCmpResult) {
            s = utils::Status::error("udfmeta not identical");
            break;
          }
        }
        const utils::TimeType &deadline = enableTTL? ttl + curTime : store::NEVER_EXPIRE;
        const utils::TimeType &updateTime = utils::TimeUtil::secondsSinceEpoch();
        if (enableTTL) {
          s = tempStore.writeTTLKV(key, value, store::VersionStore::kInvalidVersion, ttl, deadline,
              updateTime, udfMeta);
        } else {
          s = tempStore.writeKV(key, value, store::VersionStore::kInvalidVersion, updateTime, udfMeta);
        }
        SPDLOG_DEBUG("debug: exe put cmd in transaction, key {}, value size {}", key, value.size());
        if (!udfMeta.uintfield().empty()) {
          SPDLOG_DEBUG("debug: udfMeta provided for put cmd in transaction, key {}, value size {}", key, value.size());
          for (auto &val : udfMeta.uintfield()) {
            SPDLOG_DEBUG("debug: uintfield {} in udfMeta", val);
          }
        }
        events->push_back(std::make_shared<WriteEvent>(key, value, enableTTL, ttl, deadline,
            updateTime, udfMeta));
      } else if (readOrWrite.has_readentry()) {
        const store::KeyType &key = readOrWrite.readentry().key();
        store::ValueType value;
        store::TTLType ttl = store::INFINITE_TTL;
        store::VersionType version = store::VersionStore::kInvalidVersion;
        /// the value we read may not be committed at this moment
        /// but the ReadEvent generated will wait until it it commited and reply to clients
        s = this->readKV(std::make_shared<store::TemporalKVStore>(tempStore), isFollowerReadCmd,
            key, &value, &ttl, &version);
        SPDLOG_DEBUG("debug: exe get cmd in transaction, key {}, value size {}, newVersion {}, isFound {}", key,
            value.size(), version, !s.isNotFound());
        if (s.isNotFound()) {
          events->push_back(std::make_shared<ReadEvent>(key, value, ttl, version, true));
        } else if (s.isOK()) {
          if (req.ismetareturned()) {
            proto::Meta meta;
            auto m_s = this->readMeta(kvStore, isFollowerReadCmd, key, &meta);
            if (m_s.isNotFound() || !meta.has_udfmeta()) {
              events->push_back(std::make_shared<ReadEvent>(key, value, ttl, version, false));
            } else {
              events->push_back(std::make_shared<ReadEvent>(key, value, ttl, version, false,
                  meta.updatetime(), meta.udfmeta()));
            }
          } else {
            events->push_back(std::make_shared<ReadEvent>(key, value, ttl, version, false));
          }
        } else {
          break;
        }
      } else if (readOrWrite.has_removeentry()) {
        const store::KeyType &key = readOrWrite.removeentry().key();
        uint64_t targetVersion = readOrWrite.removeentry().version();
        store::ValueType deletedValue;
        store::TTLType deletedTTL = store::INFINITE_TTL;
        store::VersionType deletedVersion = store::VersionStore::kInvalidVersion;
        s = tempStore.readKV(key, &deletedValue, &deletedTTL, &deletedVersion);
        SPDLOG_DEBUG("debug: exe delete cmd in transaction, key {}, value size {}, version {}, isFound {}",
          key, deletedValue.size(), deletedVersion, !s.isNotFound());
        if (s.isNotFound()) {
          events->push_back(std::make_shared<ReadEvent>(key, deletedValue, deletedTTL, deletedVersion, true));
        } else if (s.isOK()) {
          if (targetVersion != store::VersionStore::kInvalidVersion && targetVersion != deletedVersion) {
            events->push_back(std::make_shared<ReadEvent>(key, deletedValue, deletedTTL, deletedVersion, true));
          } else {
            events->push_back(std::make_shared<DeleteEvent>(key, deletedValue, deletedVersion));
          }
        }
      } else {
        s = utils::Status::invalidArg("invalid request");
        break;
      }
    }
  } while (0);
  return s;
}

utils::Status KVTransCommand::finish(const std::shared_ptr<store::KVStore> &kvStore, const EventList &events) {
  auto s = utils::Status::ok();
  for (auto &e : events) {
    s = e->apply(*kvStore);
    if (!s.isOK()) {
      break;
    }
  }
  assert(kvStore->unlock(mContext->getTargetKeys(), true).isOK());
  return s;
}

}  /// namespace goblin::kvengine::model
