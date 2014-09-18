#include <xzero/executor/ThreadedExecutor.h>
#include <xzero/net/Server.h>
#include <xzero/net/InetConnector.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpOutput.h>
#include <xzero/http/v1/HttpConnectionFactory.h>
#include <xzero/support/libev/LibevScheduler.h>
#include <xzero/support/libev/LibevSelector.h>
#include <xzero/support/libev/LibevClock.h>
#include <unistd.h>
#include <ev++.h>

int main() {
  xzero::ThreadedExecutor threadedExecutor;
  xzero::Server server;
  bool shutdown = false;

  auto inet = server.addConnector<xzero::InetConnector>(
      "http", &threadedExecutor, nullptr, nullptr,
      xzero::IPAddress("0.0.0.0"), 3000, 128, true, false);
  inet->setBlocking(true);

  auto http = inet->addConnectionFactory<xzero::http1::HttpConnectionFactory>();

  http->setHandler([](xzero::HttpRequest* request, xzero::HttpResponse* response) {
    const xzero::Buffer body = "Hello, World\n";

    request->setHandled(true);
    response->setStatus(xzero::HttpStatus::Ok);
    response->setContentLength(body.size());
    response->output()->write(body);
    response->completed();
  });

  server.start();

  while (!shutdown)
    sleep(1);

  server.stop();

  return 0;
}