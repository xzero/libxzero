#include <xzero/logging/LogAggregator.h>
#include <xzero/logging/LogSource.h>
#include <xzero/logging/LogTarget.h>

namespace xzero {

LogAggregator::LogAggregator(LogLevel logLevel, LogTarget* logTarget)
    : target_(logTarget),
      logLevel_(logLevel),
      enabledSources_() {
}

void LogAggregator::setLogTarget(LogTarget* target) {
  target_ = target;
}

void LogAggregator::registerSource(LogSource* source) {
  activeSources_[source->className()] = source;
}

void LogAggregator::unregisterSource(LogSource* source) {
  auto i = activeSources_.find(source->className());
  activeSources_.erase(i);
}

LogSource* LogAggregator::findSource(const std::string& className) const {
  auto i = activeSources_.find(className);

  if (i != activeSources_.end())
    return i->second;
  else
    return nullptr;
}

LogAggregator& LogAggregator::get() {
  static LogAggregator aggregator;
  return aggregator;
}

} // namespace xzero