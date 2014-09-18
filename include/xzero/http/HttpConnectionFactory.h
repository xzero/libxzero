#pragma once

#include <xzero/Api.h>
#include <xzero/net/ConnectionFactory.h>
#include <xzero/http/HttpHandler.h>
#include <xzero/http/HttpDateGenerator.h>
#include <memory>

namespace xzero {

class WallClock;

/**
 * Base HTTP connection factory.
 *
 * This provides common functionality to all HTTP connection factories.
 */
class XZERO_API HttpConnectionFactory : public ConnectionFactory {
 public:
  explicit HttpConnectionFactory(const std::string& protocolName);
  ~HttpConnectionFactory();

  const HttpHandler& handler() const { return handler_; }
  void setHandler(HttpHandler&& handler);

  WallClock* clock() const { return clock_; }
  void setClock(WallClock* clock);

  HttpDateGenerator* dateGenerator() const { return dateGenerator_.get(); }

  Connection* configure(Connection* connection, Connector* connector) override;

 private:
  HttpHandler handler_;
  WallClock* clock_;
  std::unique_ptr<HttpDateGenerator> dateGenerator_;
};

}  // namespace xzero