/**
 * @file cas_client_task.h
 * @brief Asynchronous CAS validation task built on bsrvcore::HttpClientTask.
 * @author Haoming Bai <haomingbai@hotmail.com>
 * @date   2026-04-09
 *
 * Copyright (c) 2026 Haoming Bai
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifndef BCAS_CLIENT_CAS_CLIENT_TASK_H_
#define BCAS_CLIENT_CAS_CLIENT_TASK_H_

#include <functional>
#include <memory>

#include "bcas/client/cas_types.h"
#include "bsrvcore/core/trait.h"
#include "bsrvcore/core/types.h"

namespace bcas::client {

/**
 * @brief One-shot asynchronous CAS validation task.
 *
 * The task internally creates a bsrvcore::HttpClientTask, parses the returned
 * CAS XML payload, and finally delivers a single @ref CasClientResult to the
 * registered completion callback.
 */
class CasClientTask : public std::enable_shared_from_this<CasClientTask>,
                      public bsrvcore::NonCopyableNonMovable<CasClientTask> {
 public:
  /** @brief Executor type accepted by factory functions. */
  using Executor = bsrvcore::IoContextExecutor;
  /** @brief Final completion callback type. */
  using Callback = std::function<void(const CasClientResult&)>;

  /**
   * @brief Create a validation task using the same executor for I/O and
   * callbacks.
   * @param io_executor Executor used to drive the underlying HTTP task.
   * @param params Validation request parameters.
   * @return Shared task handle.
   */
  static std::shared_ptr<CasClientTask> Create(Executor io_executor,
                                               CasClientParams params);

  /**
   * @brief Create a validation task with a dedicated callback executor.
   * @param io_executor Executor used for transport work.
   * @param callback_executor Executor used to run the user callback.
   * @param params Validation request parameters.
   * @return Shared task handle.
   */
  static std::shared_ptr<CasClientTask> Create(Executor io_executor,
                                               Executor callback_executor,
                                               CasClientParams params);

  /**
   * @brief Create a validation task with a caller-supplied SSL context.
   * @param io_executor Executor used for transport work.
   * @param ssl_ctx Shared SSL context used when the base URL is HTTPS.
   * @param params Validation request parameters.
   * @return Shared task handle.
   */
  static std::shared_ptr<CasClientTask> Create(Executor io_executor,
                                               bsrvcore::SslContextPtr ssl_ctx,
                                               CasClientParams params);

  /**
   * @brief Create a validation task with dedicated callback executor and SSL
   * context.
   * @param io_executor Executor used for transport work.
   * @param callback_executor Executor used to run the user callback.
   * @param ssl_ctx Shared SSL context used when the base URL is HTTPS.
   * @param params Validation request parameters.
   * @return Shared task handle.
   */
  static std::shared_ptr<CasClientTask> Create(Executor io_executor,
                                               Executor callback_executor,
                                               bsrvcore::SslContextPtr ssl_ctx,
                                               CasClientParams params);

  /**
   * @brief Register the final completion callback.
   * @param cb Callback invoked at most once.
   * @return `shared_from_this()` for fluent setup.
   */
  std::shared_ptr<CasClientTask> OnDone(Callback cb);

  /**
   * @brief Start the validation task.
   *
   * Repeated calls are ignored.
   */
  void Start();

  /**
   * @brief Cancel the validation task.
   *
   * If the underlying HTTP task has already started, cancellation is forwarded
   * to it. If Start() has not yet been called, the task completes immediately
   * with @ref CasTaskStatus::kCancelled when started.
   */
  void Cancel();

  /**
   * @brief Inspect the immutable task parameters.
   * @return Task parameters originally supplied to Create().
   */
  [[nodiscard]] const CasClientParams& Params() const noexcept;

  ~CasClientTask();

 private:
  class Impl;

  explicit CasClientTask(std::shared_ptr<Impl> impl);

  std::shared_ptr<Impl> impl_;
};

}  // namespace bcas::client

#endif  // BCAS_CLIENT_CAS_CLIENT_TASK_H_
