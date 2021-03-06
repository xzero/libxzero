// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-http/HeaderFieldList.h>
#include <xzero-http/HttpStatus.h>
#include <xzero-base/net/Connection.h>
#include <xzero-base/CompletionHandler.h>
#include <xzero-base/Buffer.h>
#include <memory>

namespace xzero {

class FileRef;
class HttpResponseInfo;

/**
 * HTTP transport layer API.
 *
 * Implemements the wire transport protocol for HTTP messages without
 * any semantics.
 *
 * For HTTP/1 for example it is <b>RFC 7230</b>.
 */
class XZERO_HTTP_API HttpTransport : public Connection {
 public:
  HttpTransport(EndPoint* endpoint, Executor* executor);

  /**
   * Cancels communication completely.
   */
  virtual void abort() = 0;

  /**
   * Invoked when the currently generated response has been fully transmitted.
   */
  virtual void completed() = 0;

  /**
   * Initiates sending a response to the client.
   *
   * @param responseInfo HTTP response meta informations.
   * @param chunk response body data chunk.
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   *
   * @note You must ensure the data chunk is available until sending completed!
   */
  virtual void send(HttpResponseInfo&& responseInfo, const BufferRef& chunk,
                    CompletionHandler onComplete) = 0;

  /**
   * Initiates sending a response to the client.
   *
   * @param responseInfo HTTP response meta informations.
   * @param chunk response body data chunk.
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   */
  virtual void send(HttpResponseInfo&& responseInfo, Buffer&& chunk,
                    CompletionHandler onComplete) = 0;

  /**
   * Initiates sending a response to the client.
   *
   * @param responseInfo HTTP response meta informations.
   * @param chunk response body chunk represented as a file.
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   */
  virtual void send(HttpResponseInfo&& responseInfo, FileRef&& chunk,
                    CompletionHandler onComplete) = 0;

  /**
   * Transfers this data chunk to the output stream.
   *
   * @param chunk response body chunk
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   */
  virtual void send(Buffer&& chunk, CompletionHandler onComplete) = 0;

  /**
   * Transfers this file data chunk to the output stream.
   *
   * @param chunk response body chunk represented as a file.
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   */
  virtual void send(FileRef&& chunk, CompletionHandler onComplete) = 0;

  /**
   * Transfers this data chunk to the output stream.
   *
   * @param chunk response body chunk
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   */
  virtual void send(const BufferRef& chunk, CompletionHandler onComplete) = 0;
};

inline HttpTransport::HttpTransport(EndPoint* endpoint,
                                    Executor* executor)
    : Connection(endpoint, executor) {
}

}  // namespace xzero2
