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

#include "BaseCallData.h"
#include "../../module/Base.h"

namespace goblin::objectmanager::network {
  BaseCallData::BaseCallData(goblin::proto::KVManager::AsyncService *service,
                             grpc::ServerCompletionQueue *cq,
                             std::shared_ptr<kvengine::KVEngine> kvEngine) : RequestCallData(service, cq, kvEngine) {
  }

  grpc::Status BaseCallData::buildStatus(const proto::ResponseCode& code) {
    switch (code) {
      case proto::ResponseCode::PRECOND_NOT_MATCHED:
        return grpc::Status(grpc::StatusCode::OK, "precondition check failed, please try again");
      case proto::ResponseCode::NOT_LEADER:
        return grpc::Status(grpc::StatusCode::OK, "Not leader any more, please try another node");
      case proto::ResponseCode::BAD_REQUEST:
        return grpc::Status(grpc::StatusCode::OK, "Bad request found, please contact admin");
      case proto::ResponseCode::GENERAL_ERROR:
        return grpc::Status(grpc::StatusCode::OK, "General error found, please contact admin");
      default:
        return grpc::Status::OK;
    }
  }

  grpc::Status BaseCallData::okStatus(const std::string& msg) {
    return grpc::Status(grpc::StatusCode::OK, msg);
  }

  void BaseCallData::buildHeader(proto::ResponseHeader &header, proto::ResponseCode code, const std::string &msg) {
    module::Base::buildHeader(header, code, msg);
  }
}  ///  end namespace goblin::objectmanager::network
