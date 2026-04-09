/**
 * @file cas_client_task.cc
 * @brief Async CAS validation task implementation.
 * @author Haoming Bai <haomingbai@hotmail.com>
 * @date   2026-04-09
 *
 * Copyright (c) 2026 Haoming Bai
 * SPDX-License-Identifier: MIT
 */

#include "bcas/client/cas_client_task.h"

#include <boost/asio/post.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/verb.hpp>
#include <memory>
#include <mutex>
#include <utility>

#include "client/cas_detail.h"
#include "bsrvcore/connection/client/http_client_task.h"

namespace bcas::client {

namespace http = boost::beast::http;

class CasClientTask::Impl : public std::enable_shared_from_this<Impl> {
 public:
  Impl(Executor io_executor, Executor callback_executor,
       bsrvcore::SslContextPtr ssl_ctx, CasClientParams params)
      : io_executor_(std::move(io_executor)),
        callback_executor_(std::move(callback_executor)),
        ssl_ctx_(std::move(ssl_ctx)),
        params_(std::move(params)) {}

  std::shared_ptr<Impl> OnDone(Callback cb) {
    std::scoped_lock const lock(mutex_);
    on_done_ = std::move(cb);
    return shared_from_this();
  }

  void Start() {
    std::string request_url;
    std::string error_message;
    {
      std::scoped_lock const lock(mutex_);
      if (started_) {
        return;
      }
      started_ = true;
      if (cancel_requested_) {
        PostCompletion(MakeCancelledResult());
        return;
      }

      request_url = detail::BuildValidateUrl(params_, &error_message);
      if (request_url.empty()) {
        PostCompletion(MakeImmediateFailure(CasTaskStatus::kInvalidArgument,
                                            std::move(error_message)));
        return;
      }
    }

    std::shared_ptr<bsrvcore::HttpClientTask> inner;
    if (ssl_ctx_ != nullptr && detail::IsHttpsUrl(request_url)) {
      inner = bsrvcore::HttpClientTask::CreateFromUrl(
          io_executor_, callback_executor_, ssl_ctx_, request_url,
          http::verb::get, params_.http_options);
    } else {
      inner = bsrvcore::HttpClientTask::CreateFromUrl(
          io_executor_, callback_executor_, request_url, http::verb::get,
          params_.http_options);
    }

    inner->Request().set(http::field::accept, "application/xml, text/xml");
    inner->OnDone([self = shared_from_this()](const bsrvcore::HttpClientResult& result) {
      self->HandleHttpDone(result);
    });

    {
      std::scoped_lock const lock(mutex_);
      inner_task_ = inner;
    }
    inner->Start();
  }

  void Cancel() {
    std::shared_ptr<bsrvcore::HttpClientTask> inner;
    {
      std::scoped_lock const lock(mutex_);
      if (completed_) {
        return;
      }

      if (!started_) {
        cancel_requested_ = true;
        return;
      }
      inner = inner_task_;
    }

    if (inner != nullptr) {
      inner->Cancel();
    }
  }

  [[nodiscard]] const CasClientParams& Params() const noexcept {
    return params_;
  }

 private:
  CasClientResult MakeImmediateFailure(CasTaskStatus status,
                                       std::string error_message) const {
    CasClientResult result;
    result.status = status;
    result.failure_message = std::move(error_message);
    return result;
  }

  CasClientResult MakeCancelledResult() const {
    CasClientResult result;
    result.status = CasTaskStatus::kCancelled;
    result.cancelled = true;
    result.failure_message = "request cancelled";
    return result;
  }

  void PostCompletion(CasClientResult result) {
    auto self = shared_from_this();
    boost::asio::post(callback_executor_,
                      [self = std::move(self),
                       result = std::move(result)]() mutable {
                        self->Complete(std::move(result));
                      });
  }

  void HandleHttpDone(const bsrvcore::HttpClientResult& result) {
    Complete(detail::ConvertValidationResult(result));
  }

  void Complete(CasClientResult result) {
    Callback cb;
    {
      std::scoped_lock const lock(mutex_);
      if (completed_) {
        return;
      }
      completed_ = true;
      cb = on_done_;
      inner_task_.reset();
    }

    if (cb) {
      cb(result);
    }
  }

  Executor io_executor_;
  Executor callback_executor_;
  bsrvcore::SslContextPtr ssl_ctx_;
  CasClientParams params_;

  mutable std::mutex mutex_;
  Callback on_done_;
  std::shared_ptr<bsrvcore::HttpClientTask> inner_task_;
  bool started_{false};
  bool completed_{false};
  bool cancel_requested_{false};
};

std::shared_ptr<CasClientTask> CasClientTask::Create(Executor io_executor,
                                                     CasClientParams params) {
  return Create(io_executor, io_executor, nullptr, std::move(params));
}

std::shared_ptr<CasClientTask> CasClientTask::Create(Executor io_executor,
                                                     Executor callback_executor,
                                                     CasClientParams params) {
  return Create(std::move(io_executor), std::move(callback_executor), nullptr,
                std::move(params));
}

std::shared_ptr<CasClientTask> CasClientTask::Create(
    Executor io_executor, bsrvcore::SslContextPtr ssl_ctx,
    CasClientParams params) {
  return Create(io_executor, io_executor, std::move(ssl_ctx),
                std::move(params));
}

std::shared_ptr<CasClientTask> CasClientTask::Create(
    Executor io_executor, Executor callback_executor,
    bsrvcore::SslContextPtr ssl_ctx, CasClientParams params) {
  auto impl = std::make_shared<Impl>(std::move(io_executor),
                                     std::move(callback_executor),
                                     std::move(ssl_ctx), std::move(params));
  return std::shared_ptr<CasClientTask>(new CasClientTask(std::move(impl)));
}

std::shared_ptr<CasClientTask> CasClientTask::OnDone(Callback cb) {
  impl_->OnDone(std::move(cb));
  return shared_from_this();
}

void CasClientTask::Start() { impl_->Start(); }

void CasClientTask::Cancel() { impl_->Cancel(); }

const CasClientParams& CasClientTask::Params() const noexcept {
  return impl_->Params();
}

CasClientTask::~CasClientTask() = default;

CasClientTask::CasClientTask(std::shared_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

}  // namespace bcas::client
