#include <xzero/http/v1/HttpConnection.h>
#include <xzero/http/v1/HttpChannel.h>
#include <xzero/http/v1/HttpInput.h>
#include <xzero/http/HttpDateGenerator.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/net/Connection.h>
#include <xzero/net/EndPoint.h>
#include <xzero/net/EndPointWriter.h>
#include <xzero/WallClock.h>
#include <xzero/sysconfig.h>
#include <cassert>
#include <cstdlib>

namespace xzero {
namespace http1 {

HttpConnection::HttpConnection(std::shared_ptr<EndPoint> endpoint,
                               const HttpHandler& handler,
                               HttpDateGenerator* dateGenerator)
    : HttpTransport(endpoint),
      parser_(HttpParser::REQUEST),
      generator_(dateGenerator),
      inputBuffer_(),
      inputOffset_(0),
      writer_(),
      onComplete_(),
      channel_(new HttpChannel(
          this, handler, std::unique_ptr<HttpInput>(new HttpInput(this)))),
      maxKeepAlive_(TimeSpan::fromSeconds(60)),  // TODO: parametrize me
      requestCount_(0),
      requestMax_(100) {

  parser_.setListener(channel_.get());
}

HttpConnection::~HttpConnection() {
  /* printf("~HttpConnection\n"); */
}

void HttpConnection::onOpen() {
  HttpTransport::onOpen();

#if 0
  if (connector()->deferAccept())
    onFillable();
  else
    wantFill();
#else
  wantFill();
#endif
}

void HttpConnection::onClose() {
  HttpTransport::onClose();
}

void HttpConnection::abort() {
  endpoint()->close();
}

void HttpConnection::completed() {
  if (onComplete_)
    throw std::runtime_error("there is still another completion hook.");

  generator_.generateBody(BufferRef(), true, &writer_);  // EOS

  onComplete_ = [this](bool) {
    if (channel_->isPersistent()) {
      // re-use on keep-alive
      channel_->reset();

      if (endpoint()->isCorking()) endpoint()->setCorking(false);

      if (inputOffset_ < inputBuffer_.size()) {
        // have some request pipelined
        onFillable();
      } else {
        // wait for next request
        wantFill();
      }
    } else {
      endpoint()->close();
    }
  };

  wantFlush();
}

void HttpConnection::send(HttpResponseInfo&& responseInfo,
                          const BufferRef& chunk,
                          CompletionHandler&& onComplete) {
  if (onComplete && onComplete_)
    throw std::runtime_error("there is still another completion hook.");

  // printf("HttpConnection.send(status=%d, chunkSize=%zu)\n",
  //        responseInfo.status(), chunk.size());

  // patch in HTTP transport-layer headers
  if (channel_->isPersistent() && requestCount_ < requestMax_) {
    ++requestCount_;

    char keepAlive[64];
    snprintf(keepAlive, sizeof(keepAlive), "timeout=%lu, max=%zu",
             maxKeepAlive_.totalSeconds(), requestMax_ - requestCount_);

    responseInfo.headers().push_back("Connection", "keep-alive");
    responseInfo.headers().push_back("Keep-Alive", keepAlive);
  } else {
    channel_->setPersistent(false);
    responseInfo.headers().push_back("Connection", "closed");
  }

  // add server related headers
  responseInfo.headers().push_back("Server", "xzero/" LIBXZERO_VERSION);

  generator_.generateResponse(responseInfo, chunk, false, &writer_);
  onComplete_ = onComplete;

  const bool corking_ = true;  // TODO(TCP_CORK): part of HttpResponseInfo?
  if (corking_) endpoint()->setCorking(true);

  wantFlush();
}

void HttpConnection::send(const BufferRef& chunk,
                          CompletionHandler&& onComplete) {
  if (onComplete && onComplete_)
    throw std::runtime_error("there is still another completion hook.");

  generator_.generateBody(chunk, false, &writer_);
  onComplete_ = std::move(onComplete);

  wantFlush();
}

void HttpConnection::send(FileRef&& chunk, CompletionHandler&& onComplete) {
  if (onComplete && onComplete_)
    throw std::runtime_error("there is still another completion hook.");

  generator_.generateBody(std::forward<FileRef>(chunk), false, &writer_);
  onComplete_ = std::move(onComplete);
  wantFlush();
}

void HttpConnection::setInputBufferSize(size_t size) {
  inputBuffer_.reserve(size);
}

void HttpConnection::onFillable() {
  if (endpoint()->fill(&inputBuffer_) == 0) {
    abort();  // throw std::runtime_error("client EOF");
  }

  size_t n = parser_.parseFragment(inputBuffer_.ref(inputOffset_));
  inputOffset_ += n;

  channel_->run();
}

void HttpConnection::onFlushable() {
  const bool complete = writer_.flush(endpoint());
  if (complete) {
    if (onComplete_) {
      auto callback = std::move(onComplete_);
      callback(true);
    }
  }
}

}  // namespace http1
}  // namespace xzero