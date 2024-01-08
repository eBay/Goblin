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

#include <spdlog/spdlog.h>
#include <grpcpp/server_builder.h>

#include <utility>

#include <infra/common_types.h>

#include "RequestCallData.h"
#include "RequestReceiver.h"
#include "rpc/RouterCallData.h"
#include "rpc/OnStartupCallData.h"
#include "rpc/OnMigrationCallData.h"
#include "rpc/AddClusterCallData.h"
#include "rpc/RemoveClusterCallData.h"

namespace goblin::objectmanager::network {

  RequestReceiver::RequestReceiver(
          const INIReader &reader,
          std::shared_ptr<kvengine::KVEngine> kvEngine) :
          mKVEngine(kvEngine) {
    mIpPort = reader.Get("receiver", "ip.port", "UNKNOWN");
    assert(mIpPort != "UNKNOWN");

    mTlsConfOpt = kvengine::utils::TlsUtil::parseTlsConf(reader, "tls");
  }

  void RequestReceiver::run() {
    if (mIsShutdown) {
      SPDLOG_WARN("Receiver is already down. Will not run again.");
      return;
    }

    std::string server_address(mIpPort);

    grpc::ServerBuilder builder;
    builder.SetMaxReceiveMessageSize(4 << 20);
    builder.AddListeningPort(server_address,
                             kvengine::utils::TlsUtil::buildServerCredentials(mTlsConfOpt));
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&mService);

    // Finally assemble the server.
    for (uint64_t i = 0; i < kConcurrency; ++i) {
      mCompletionQueues.emplace_back(builder.AddCompletionQueue());
    }

    mServer = builder.BuildAndStart();
    SPDLOG_INFO("Server listening on {}", server_address);

    // Spawn a new CallData instance to serve new clients.
    for (uint64_t i = 0; i < 2000; ++i) {
      for (uint64_t j = 0; j < kConcurrency; ++j) {
        new RouterCallData(&mService, mCompletionQueues[j].get(), mKVEngine);
        new OnStartupCallData(&mService, mCompletionQueues[j].get(), mKVEngine);
        new OnMigrationCallData(&mService, mCompletionQueues[j].get(), mKVEngine);
        new AddClusterCallData(&mService, mCompletionQueues[j].get(), mKVEngine);
        new RemoveClusterCallData(&mService, mCompletionQueues[j].get(), mKVEngine);
      }
    }

    for (uint64_t i = 0; i < kConcurrency; ++i) {
      mRcvThreads.emplace_back([this, i]() {
        std::string threadName = (std::string("RcvThread_") + std::to_string(i));
        pthread_setname_np(pthread_self(), threadName.c_str());
        handleRpcs(i);
      });
    }
  }

  void RequestReceiver::handleRpcs(uint64_t i) {
    // SPDLOG_INFO("debug: do handleRpcs");
    void *tag;  // uniquely identifies a request.
    bool ok;
    // Block waiting to read the next event from the completion queue. The
    // event is uniquely identified by its tag, which in this case is the
    // memory address of a CallData instance.
    // The return value of Next should always be checked. This return value
    // tells us whether there is any kind of event or cq_ is shutting down.
    while (mCompletionQueues[i]->Next(&tag, &ok)) {
      auto *callData = static_cast<RequestCallData *>(tag);
      if (ok) {
        callData->proceed();
      } else {
        callData->failOver();
      }
    }
  }

  void RequestReceiver::join() {
    for (uint64_t i = 0; i < kConcurrency; ++i) {
      mRcvThreads[i].join();
    }
  }

  void RequestReceiver::shutdown() {
    SPDLOG_INFO("Shutting down Receiver");
    if (mIsShutdown) {
      SPDLOG_INFO("Server is already down");
    } else {
      mIsShutdown = true;
      mServer->Shutdown();

      for (uint64_t i = 0; i < kConcurrency; ++i) {
        mCompletionQueues[i]->Shutdown();
      }
    }
  }

}  /// namespace goblin::objectmanager::network
