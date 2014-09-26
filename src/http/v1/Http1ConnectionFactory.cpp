#include <xzero/http/v1/Http1ConnectionFactory.h>
#include <xzero/http/v1/HttpConnection.h>

namespace xzero {
namespace http1 {

Http1ConnectionFactory::Http1ConnectionFactory(
    size_t maxRequestUriLength,
    size_t maxRequestBodyLength,
    size_t maxRequestCount,
    TimeSpan maxKeepAlive)
    : HttpConnectionFactory("http", maxRequestUriLength, maxRequestBodyLength),
      maxRequestCount_(maxRequestCount),
      maxKeepAlive_(maxKeepAlive) {
  setInputBufferSize(16 * 1024);
}

Http1ConnectionFactory::~Http1ConnectionFactory() {
}

Connection* Http1ConnectionFactory::create(Connector* connector,
                                           std::shared_ptr<EndPoint> endpoint) {
  return configure(new http1::HttpConnection(endpoint, handler(),
                                             dateGenerator(),
                                             maxRequestUriLength(),
                                             maxRequestBodyLength(),
                                             maxRequestCount(),
                                             maxKeepAlive()),
                   connector);
}

}  // namespace http1
}  // namespace xzero