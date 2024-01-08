/**
 * Copyright (c) 2020 eBay Software Foundation. All rights reserved.
 */


#include <spdlog/spdlog.h>

#include "TransferCommand.h"

namespace goblin::objectstore::model {
  kvengine::utils::Status TransferCommand::generate(
      const std::map<kvengine::store::KeyType,
                    std::pair<kvengine::store::ValueType,
                    kvengine::store::VersionType>> &kvsToRead,
      const kvengine::model::InputInfoType &inputInfo,
      std::map<kvengine::store::KeyType, kvengine::store::ValueType> *kvsToWrite,
      kvengine::model::OutputInfoType *outputInfo) {
    SPDLOG_INFO("debug: running transfer cmd");
    proto::TransferCommand cmd;
    auto parseRes = cmd.ParseFromString(inputInfo);
    if (!parseRes) {
      return kvengine::utils::Status::invalidArg("failed to parse inputinfo");
    }
    auto fromAcc = cmd.fromaccount();
    auto toAcc = cmd.toaccount();
    auto balance = cmd.balance();
    SPDLOG_INFO("debug: transfer balance {} from {} to {}", balance, fromAcc, toAcc);
    uint64_t balanceInFrom = 0;
    uint64_t balanceInTo = 0;
    if (kvsToRead.find(fromAcc) != kvsToRead.end()) {
      balanceInFrom = std::stoi(kvsToRead.at(fromAcc).first);
    }
    if (kvsToRead.find(toAcc) != kvsToRead.end()) {
      balanceInTo = std::stoi(kvsToRead.at(toAcc).first);
    }
    /// just as an example, we have to check negative balance in real business
    (*kvsToWrite)[fromAcc] = std::to_string(balanceInFrom - balance);
    (*kvsToWrite)[toAcc] = std::to_string(balanceInTo + balance);
    *outputInfo = "transferDemoOutputInfo";
    return kvengine::utils::Status::ok();
  }
}  /// namespace goblin::objectstore::model
