// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-http/HttpTransport.h>
#include <xzero-http/HttpHandler.h>
#include <xzero-http/HttpResponseInfo.h>
#include <xzero-http/HttpChannel.h>
#include <xzero-base/Buffer.h>
#include <xzero-http/Api.h>
#include <xzero-base/sysconfig.h>

namespace xzero {

class Executor;

/**
 * Mock HTTP Transport, used to create mock requests.
 *
 * This HTTP transport implementation is not using byte streams to communicate
 * but high level data structures to create requests and provides
 * access to high level data structures to read out the response message.
 *
 * The Executor service is only used for completion handlers.
 *
 * The actual HTTP request message handler is custom passed to the constructor.
 *
 * @note This object is not thread safe.
 *
 * @see HttpResponseInfo
 * @see HttpHandler
 * @see Executor
 */
class XZERO_HTTP_API MockTransport : public HttpTransport {
 public:
  /**
   * Initializes the mock transport object.
   *
   * @param executor The executor service used for completion handlers.
   * @param handler The handler to run for incoming HTTP request messages.
   */
  MockTransport(Executor* executor, const HttpHandler& handler);

  /**
   * Initializes the mock transport object.
   *
   * @param executor The executor service used for completion handlers.
   * @param handler The handler to run for incoming HTTP request messages.
   * @param maxRequestUriLength Maximum request URI length.
   * @param maxRequestBodyLength Maximum request body length.
   * @param outputCompressor HTTP response body compression service.
   */
  MockTransport(Executor* executor,
                const HttpHandler& handler,
                size_t maxRequestUriLength,
                size_t maxRequestBodyLength,
                HttpOutputCompressor* outputCompressor);

  ~MockTransport();

  /**
   * Runs given HTTP request message.
   *
   * @param version HTTP message version, such as @c HttpVersion::VERSION_1_1.
   * @param method HTTP request method, such as @c "GET".
   * @param entity HTTP request entity, such as @c "/index.html".
   * @param headers HTTP request headers.
   * @param body HTTP request body.
   *
   * @see responseInfo()
   * @see responseBody()
   */
  void run(HttpVersion version, const std::string& method,
           const std::string& entity, const HeaderFieldList& headers,
           const std::string& body = "");

  /** Retrieves the response message status line and headers. */
  const HttpResponseInfo& responseInfo() const XZERO_NOEXCEPT;

  /** Retrieves the response message body. */
  const Buffer& responseBody() const XZERO_NOEXCEPT;

  /** Tests whether this transport was aborted in last request handling. */
  bool isAborted() const XZERO_NOEXCEPT;

  /** Tests whether last message was completed. */
  bool isCompleted() const XZERO_NOEXCEPT;

 private:
  // HttpTransport overrides
  void abort() override;
  void completed() override;

  void send(HttpResponseInfo&& responseInfo, const BufferRef& chunk,
            CompletionHandler onComplete) override;
  void send(HttpResponseInfo&& responseInfo, Buffer&& chunk,
            CompletionHandler onComplete) override;
  void send(HttpResponseInfo&& responseInfo, FileRef&& chunk,
            CompletionHandler onComplete) override;

  void send(const BufferRef& chunk, CompletionHandler onComplete) override;
  void send(Buffer&& chunk, CompletionHandler onComplete) override;
  void send(FileRef&& chunk, CompletionHandler onComplete) override;

  // Connection overrides
  void onOpen() override;
  void onClose() override;
  void onFillable() override;
  void onFlushable() override;
  bool onReadTimeout() override;

 private:
  HttpHandler handler_;
  size_t maxRequestUriLength_;
  size_t maxRequestBodyLength_;
  HttpOutputCompressor* outputCompressor_;

  bool isAborted_;
  bool isCompleted_;
  std::unique_ptr<HttpChannel> channel_;
  HttpResponseInfo responseInfo_;
  Buffer responseBody_;
};

// {{{ inlines
inline const HttpResponseInfo& MockTransport::responseInfo() const XZERO_NOEXCEPT {
  return responseInfo_;
}

inline const Buffer& MockTransport::responseBody() const XZERO_NOEXCEPT {
  return responseBody_;
}

inline bool MockTransport::isAborted() const XZERO_NOEXCEPT {
  return isAborted_;
}

inline bool MockTransport::isCompleted() const XZERO_NOEXCEPT {
  return isCompleted_;
}
// }}}

} // namespace xzero
