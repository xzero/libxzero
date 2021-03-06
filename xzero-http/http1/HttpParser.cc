// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-http/http1/HttpParser.h>
#include <xzero-http/HttpListener.h>
#include <xzero-base/logging.h>

namespace xzero {
namespace http1 {

#if 0 //!defined(NDEBUG)
#define TRACE(level, fmt...) XZERO_DEBUG("http-parser", level, fmt)
#else
#define TRACE(level, msg...) \
  do {                       \
  } while (0)
#endif

std::string to_string(HttpParser::State state) {
  switch (state) {
    // artificial
    case HttpParser::PROTOCOL_ERROR:
      return "protocol-error";
    case HttpParser::MESSAGE_BEGIN:
      return "message-begin";

    // request-line
    case HttpParser::REQUEST_LINE_BEGIN:
      return "request-line-begin";
    case HttpParser::REQUEST_METHOD:
      return "request-method";
    case HttpParser::REQUEST_ENTITY_BEGIN:
      return "request-entity-begin";
    case HttpParser::REQUEST_ENTITY:
      return "request-entity";
    case HttpParser::REQUEST_PROTOCOL_BEGIN:
      return "request-protocol-begin";
    case HttpParser::REQUEST_PROTOCOL_T1:
      return "request-protocol-t1";
    case HttpParser::REQUEST_PROTOCOL_T2:
      return "request-protocol-t2";
    case HttpParser::REQUEST_PROTOCOL_P:
      return "request-protocol-p";
    case HttpParser::REQUEST_PROTOCOL_SLASH:
      return "request-protocol-slash";
    case HttpParser::REQUEST_PROTOCOL_VERSION_MAJOR:
      return "request-protocol-version-major";
    case HttpParser::REQUEST_PROTOCOL_VERSION_MINOR:
      return "request-protocol-version-minor";
    case HttpParser::REQUEST_LINE_LF:
      return "request-line-lf";
    case HttpParser::REQUEST_0_9_LF:
      return "request-0-9-lf";

    // Status-Line
    case HttpParser::STATUS_LINE_BEGIN:
      return "status-line-begin";
    case HttpParser::STATUS_PROTOCOL_BEGIN:
      return "status-protocol-begin";
    case HttpParser::STATUS_PROTOCOL_T1:
      return "status-protocol-t1";
    case HttpParser::STATUS_PROTOCOL_T2:
      return "status-protocol-t2";
    case HttpParser::STATUS_PROTOCOL_P:
      return "status-protocol-t2";
    case HttpParser::STATUS_PROTOCOL_SLASH:
      return "status-protocol-t2";
    case HttpParser::STATUS_PROTOCOL_VERSION_MAJOR:
      return "status-protocol-version-major";
    case HttpParser::STATUS_PROTOCOL_VERSION_MINOR:
      return "status-protocol-version-minor";
    case HttpParser::STATUS_CODE_BEGIN:
      return "status-code-begin";
    case HttpParser::STATUS_CODE:
      return "status-code";
    case HttpParser::STATUS_MESSAGE_BEGIN:
      return "status-message-begin";
    case HttpParser::STATUS_MESSAGE:
      return "status-message";
    case HttpParser::STATUS_MESSAGE_LF:
      return "status-message-lf";

    // message header
    case HttpParser::HEADER_NAME_BEGIN:
      return "header-name-begin";
    case HttpParser::HEADER_NAME:
      return "header-name";
    case HttpParser::HEADER_COLON:
      return "header-colon";
    case HttpParser::HEADER_VALUE_BEGIN:
      return "header-value-begin";
    case HttpParser::HEADER_VALUE:
      return "header-value";
    case HttpParser::HEADER_VALUE_LF:
      return "header-value-lf";
    case HttpParser::HEADER_VALUE_END:
      return "header-value-end";
    case HttpParser::HEADER_END_LF:
      return "header-end-lf";

    // LWS
    case HttpParser::LWS_BEGIN:
      return "lws-begin";
    case HttpParser::LWS_LF:
      return "lws-lf";
    case HttpParser::LWS_SP_HT_BEGIN:
      return "lws-sp-ht-begin";
    case HttpParser::LWS_SP_HT:
      return "lws-sp-ht";

    // message content
    case HttpParser::CONTENT_BEGIN:
      return "content-begin";
    case HttpParser::CONTENT:
      return "content";
    case HttpParser::CONTENT_ENDLESS:
      return "content-endless";
    case HttpParser::CONTENT_CHUNK_SIZE_BEGIN:
      return "content-chunk-size-begin";
    case HttpParser::CONTENT_CHUNK_SIZE:
      return "content-chunk-size";
    case HttpParser::CONTENT_CHUNK_LF1:
      return "content-chunk-lf1";
    case HttpParser::CONTENT_CHUNK_BODY:
      return "content-chunk-body";
    case HttpParser::CONTENT_CHUNK_LF2:
      return "content-chunk-lf2";
    case HttpParser::CONTENT_CHUNK_CR3:
      return "content-chunk-cr3";
    case HttpParser::CONTENT_CHUNK_LF3:
      return "content-chunk_lf3";
  }

  return "UNKNOWN";
}

inline bool HttpParser::isProcessingHeader() const {
  // XXX should we include request-line and status-line here, too?
  switch (state_) {
    case HEADER_NAME_BEGIN:
    case HEADER_NAME:
    case HEADER_COLON:
    case HEADER_VALUE_BEGIN:
    case HEADER_VALUE:
    case HEADER_VALUE_LF:
    case HEADER_VALUE_END:
    case HEADER_END_LF:
      return true;
    default:
      return false;
  }
}

inline bool HttpParser::isProcessingBody() const {
  switch (state_) {
    case CONTENT_BEGIN:
    case CONTENT:
    case CONTENT_ENDLESS:
    case CONTENT_CHUNK_SIZE_BEGIN:
    case CONTENT_CHUNK_SIZE:
    case CONTENT_CHUNK_LF1:
    case CONTENT_CHUNK_BODY:
    case CONTENT_CHUNK_LF2:
    case CONTENT_CHUNK_CR3:
    case CONTENT_CHUNK_LF3:
      return true;
    default:
      return false;
  }
}

HttpParser::HttpParser(ParseMode mode, HttpListener* listener)
    : mode_(mode),
      listener_(listener),
      state_(MESSAGE_BEGIN),
      lwsNext_(),
      lwsNull_(),
      method_(),
      entity_(),
      versionMajor_(),
      versionMinor_(),
      code_(0),
      message_(),
      name_(),
      value_(),
      chunked_(false),
      contentLength_(-1) {
  //.
}

std::size_t HttpParser::parseFragment(const BufferRef& chunk) {
  /*
   * CR               = 0x0D
   * LF               = 0x0A
   * SP               = 0x20
   * HT               = 0x09
   *
   * CRLF             = CR LF
   * LWS              = [CRLF] 1*( SP | HT )
   *
   * HTTP-message     = Request | Response
   *
   * generic-message  = start-line
   *                    *(message-header CRLF)
   *                    CRLF
   *                    [ message-body ]
   *
   * start-line       = Request-Line | Status-Line
   *
   * Request-Line     = Method SP Request-URI SP HTTP-Version CRLF
   *
   * Method           = "OPTIONS" | "GET" | "HEAD"
   *                  | "POST"    | "PUT" | "DELETE"
   *                  | "TRACE"   | "CONNECT"
   *                  | extension-method
   *
   * Request-URI      = "*" | absoluteURI | abs_path | authority
   *
   * extension-method = token
   *
   * Status-Line      = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
   *
   * HTTP-Version     = "HTTP" "/" 1*DIGIT "." 1*DIGIT
   * Status-Code      = 3*DIGIT
   * Reason-Phrase    = *<TEXT, excluding CR, LF>
   *
   * absoluteURI      = "http://" [user ':' pass '@'] hostname [abs_path] [qury]
   * abs_path         = "/" *CHAR
   * authority        = ...
   * token            = 1*<any CHAR except CTLs or seperators>
   * separator        = "(" | ")" | "<" | ">" | "@"
   *                  | "," | ";" | ":" | "\" | <">
   *                  | "/" | "[" | "]" | "?" | "="
   *                  | "{" | "}" | SP | HT
   *
   * message-header   = field-name ":" [ field-value ]
   * field-name       = token
   * field-value      = *( field-content | LWS )
   * field-content    = <the OCTETs making up the field-value
   *                    and consisting of either *TEXT or combinations
   *                    of token, separators, and quoted-string>
   *
   * message-body     = entity-body
   *                  | <entity-body encoded as per Transfer-Encoding>
   */

  const char* i = chunk.cbegin();
  const char* e = chunk.cend();

  const size_t initialOutOffset = 0;
  size_t result = initialOutOffset;
  size_t* nparsed = &result;

  // TRACE(2, "process(curState:%s): size: %ld: '%s'", to_string(state()).c_str(),
  // chunk.size(), chunk.str().c_str());
  TRACE(2, "process(curState:%s): size: %ld", to_string(state()).c_str(),
        chunk.size());

#if 0
    switch (state_) {
        case CONTENT: // fixed size content
            if (!passContent(chunk, nparsed))
                goto done;

            i += *nparsed;
            break;
        case CONTENT_ENDLESS: // endless-sized content (until stream end)
        {
            *nparsed += chunk.size();
            bool rv = onMessageContent(chunk);

            goto done;
        }
        default:
            break;
    }
#endif

  while (i != e) {
#if !defined(NDEBUG)
    if (std::isprint(*i)) {
      TRACE(3, "parse: %4ld, 0x%02X (%c),  %s", *nparsed, *i, *i,
            to_string(state()).c_str());
    } else {
      TRACE(3, "parse: %4ld, 0x%02X,     %s", *nparsed, *i,
            to_string(state()).c_str());
    }
#endif

    switch (state_) {
      case MESSAGE_BEGIN:
        contentLength_ = -1;
        switch (mode_) {
          case REQUEST:
            state_ = REQUEST_LINE_BEGIN;
            versionMajor_ = 0;
            versionMinor_ = 0;
            break;
          case RESPONSE:
            state_ = STATUS_LINE_BEGIN;
            code_ = 0;
            versionMajor_ = 0;
            versionMinor_ = 0;
            break;
          case MESSAGE:
            state_ = HEADER_NAME_BEGIN;

            // an internet message has no special top-line,
            // so we just invoke the callback right away
            if (!onMessageBegin()) goto done;

            break;
        }
        break;
      case REQUEST_LINE_BEGIN:
        if (isToken(*i)) {
          state_ = REQUEST_METHOD;
          method_ = chunk.ref(*nparsed - initialOutOffset, 1);

          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case REQUEST_METHOD:
        if (*i == SP) {
          state_ = REQUEST_ENTITY_BEGIN;
          ++*nparsed;
          ++i;
        } else if (isToken(*i)) {
          method_.shr();
          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case REQUEST_ENTITY_BEGIN:
        if (std::isprint(*i)) {
          entity_ = chunk.ref(*nparsed - initialOutOffset, 1);
          state_ = REQUEST_ENTITY;

          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case REQUEST_ENTITY:
        if (*i == SP) {
          state_ = REQUEST_PROTOCOL_BEGIN;
          ++*nparsed;
          ++i;
        } else if (std::isprint(*i)) {
          entity_.shr();
          ++*nparsed;
          ++i;
        } else if (*i == CR) {
          state_ = REQUEST_0_9_LF;
          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case REQUEST_0_9_LF:
        if (*i == LF) {
          if (!onMessageBegin(method_, entity_, 0, 9))
            goto done;

          if (!onMessageHeaderEnd())
            goto done;

          onMessageEnd();
          goto done;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case REQUEST_PROTOCOL_BEGIN:
        if (*i != 'H') {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          state_ = REQUEST_PROTOCOL_T1;
          ++*nparsed;
          ++i;
        }
        break;
      case REQUEST_PROTOCOL_T1:
        if (*i != 'T') {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          state_ = REQUEST_PROTOCOL_T2;
          ++*nparsed;
          ++i;
        }
        break;
      case REQUEST_PROTOCOL_T2:
        if (*i != 'T') {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          state_ = REQUEST_PROTOCOL_P;
          ++*nparsed;
          ++i;
        }
        break;
      case REQUEST_PROTOCOL_P:
        if (*i != 'P') {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          state_ = REQUEST_PROTOCOL_SLASH;
          ++*nparsed;
          ++i;
        }
        break;
      case REQUEST_PROTOCOL_SLASH:
        if (*i != '/') {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          state_ = REQUEST_PROTOCOL_VERSION_MAJOR;
          ++*nparsed;
          ++i;
        }
        break;
      case REQUEST_PROTOCOL_VERSION_MAJOR:
        if (*i == '.') {
          state_ = REQUEST_PROTOCOL_VERSION_MINOR;
          ++*nparsed;
          ++i;
        } else if (!std::isdigit(*i)) {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          versionMajor_ = versionMajor_ * 10 + *i - '0';
          ++*nparsed;
          ++i;
        }
        break;
      case REQUEST_PROTOCOL_VERSION_MINOR:
        if (*i == CR) {
          state_ = REQUEST_LINE_LF;
          ++*nparsed;
          ++i;
        }
        else if (!std::isdigit(*i)) {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          versionMinor_ = versionMinor_ * 10 + *i - '0';
          ++*nparsed;
          ++i;
        }
        break;
      case REQUEST_LINE_LF:
        if (*i == LF) {
          state_ = HEADER_NAME_BEGIN;
          ++*nparsed;
          ++i;

          TRACE(2, "request-line: method=%s, entity=%s, vmaj=%d, vmin=%d",
                method_.str().c_str(), entity_.str().c_str(), versionMajor_,
                versionMinor_);

          if (!onMessageBegin(method_, entity_, versionMajor_, versionMinor_)) {
            goto done;
          }
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case STATUS_LINE_BEGIN:
      case STATUS_PROTOCOL_BEGIN:
        if (*i != 'H') {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          state_ = STATUS_PROTOCOL_T1;
          ++*nparsed;
          ++i;
        }
        break;
      case STATUS_PROTOCOL_T1:
        if (*i != 'T') {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          state_ = STATUS_PROTOCOL_T2;
          ++*nparsed;
          ++i;
        }
        break;
      case STATUS_PROTOCOL_T2:
        if (*i != 'T') {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          state_ = STATUS_PROTOCOL_P;
          ++*nparsed;
          ++i;
        }
        break;
      case STATUS_PROTOCOL_P:
        if (*i != 'P') {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          state_ = STATUS_PROTOCOL_SLASH;
          ++*nparsed;
          ++i;
        }
        break;
      case STATUS_PROTOCOL_SLASH:
        if (*i != '/') {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          state_ = STATUS_PROTOCOL_VERSION_MAJOR;
          ++*nparsed;
          ++i;
        }
        break;
      case STATUS_PROTOCOL_VERSION_MAJOR:
        if (*i == '.') {
          state_ = STATUS_PROTOCOL_VERSION_MINOR;
          ++*nparsed;
          ++i;
        } else if (!std::isdigit(*i)) {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          versionMajor_ = versionMajor_ * 10 + *i - '0';
          ++*nparsed;
          ++i;
        }
        break;
      case STATUS_PROTOCOL_VERSION_MINOR:
        if (*i == SP) {
          state_ = STATUS_CODE_BEGIN;
          ++*nparsed;
          ++i;
        } else if (!std::isdigit(*i)) {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          versionMinor_ = versionMinor_ * 10 + *i - '0';
          ++*nparsed;
          ++i;
        }
        break;
      case STATUS_CODE_BEGIN:
        if (!std::isdigit(*i)) {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
          break;
        }
        state_ = STATUS_CODE;
      /* fall through */
      case STATUS_CODE:
        if (std::isdigit(*i)) {
          code_ = code_ * 10 + *i - '0';
          ++*nparsed;
          ++i;
        } else if (*i == SP) {
          state_ = STATUS_MESSAGE_BEGIN;
          ++*nparsed;
          ++i;
        } else if (*i == CR) {  // no Status-Message passed
          state_ = STATUS_MESSAGE_LF;
          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case STATUS_MESSAGE_BEGIN:
        if (isText(*i)) {
          state_ = STATUS_MESSAGE;
          message_ = chunk.ref(*nparsed - initialOutOffset, 1);
          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case STATUS_MESSAGE:
        if (isText(*i) && *i != CR && *i != LF) {
          message_.shr();
          ++*nparsed;
          ++i;
        } else if (*i == CR) {
          state_ = STATUS_MESSAGE_LF;
          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case STATUS_MESSAGE_LF:
        if (*i == LF) {
          state_ = HEADER_NAME_BEGIN;
          ++*nparsed;
          ++i;

          // TRACE(2, "status-line: HTTP/%d.%d, code=%d, message=%s",
          // versionMajor_, versionMinor_, code_, message_.str().c_str());
          if (!onMessageBegin(versionMajor_, versionMinor_, code_, message_)) {
            goto done;
          }
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case HEADER_NAME_BEGIN:
        if (isToken(*i)) {
          name_ = chunk.ref(*nparsed - initialOutOffset, 1);
          state_ = HEADER_NAME;
          ++*nparsed;
          ++i;
        } else if (*i == CR) {
          state_ = HEADER_END_LF;
          ++*nparsed;
          ++i;
        }
        else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case HEADER_NAME:
        if (isToken(*i)) {
          name_.shr();
          ++*nparsed;
          ++i;
        } else if (*i == ':') {
          state_ = LWS_BEGIN;
          lwsNext_ = HEADER_VALUE_BEGIN;
          lwsNull_ = HEADER_VALUE_END;  // only (CR LF) parsed, assume empty
                                        // value & go on with next header
          ++*nparsed;
          ++i;
        } else if (*i == CR) {
          state_ = LWS_LF;
          lwsNext_ = HEADER_COLON;
          lwsNull_ = PROTOCOL_ERROR;
          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case HEADER_COLON:
        if (*i == ':') {
          state_ = LWS_BEGIN;
          lwsNext_ = HEADER_VALUE_BEGIN;
          lwsNull_ = HEADER_VALUE_END;
          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case LWS_BEGIN:
        if (*i == CR) {
          state_ = LWS_LF;
          ++*nparsed;
          ++i;
        } else if (*i == SP || *i == HT) {
          state_ = LWS_SP_HT;
          ++*nparsed;
          ++i;
        } else if (std::isprint(*i)) {
          state_ = lwsNext_;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case LWS_LF:
        if (*i == LF) {
          state_ = LWS_SP_HT_BEGIN;
          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case LWS_SP_HT_BEGIN:
        if (*i == SP || *i == HT) {
          if (!value_.empty()) value_.shr(3);  // CR LF (SP | HT)

          state_ = LWS_SP_HT;
          ++*nparsed;
          ++i;
        } else {
          // only (CF LF) parsed so far and no 1*(SP | HT) found.
          if (lwsNull_ == PROTOCOL_ERROR) {
            onProtocolError(HttpStatus::BadRequest);
          }
          state_ = lwsNull_;
          // XXX no nparsed/i-update
        }
        break;
      case LWS_SP_HT:
        if (*i == SP || *i == HT) {
          if (!value_.empty()) value_.shr();

          ++*nparsed;
          ++i;
        } else
          state_ = lwsNext_;
        break;
      case HEADER_VALUE_BEGIN:
        if (isText(*i)) {
          value_ = chunk.ref(*nparsed - initialOutOffset, 1);
          ++*nparsed;
          ++i;
          state_ = HEADER_VALUE;
        } else if (*i == CR) {
          state_ = HEADER_VALUE_LF;
          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case HEADER_VALUE:
        if (*i == CR) {
          state_ = LWS_LF;
          lwsNext_ = HEADER_VALUE;
          lwsNull_ = HEADER_VALUE_END;
          ++*nparsed;
          ++i;
        }
        else if (isText(*i)) {
          value_.shr();
          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case HEADER_VALUE_LF:
        if (*i == LF) {
          state_ = HEADER_VALUE_END;
          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case HEADER_VALUE_END: {
        TRACE(2, "header: name='%s', value='%s'", name_.str().c_str(),
              value_.str().c_str());

        bool rv;
        if (iequals(name_, "Content-Length")) {
          contentLength_ = value_.toInt();
          TRACE(2, "set content length to: %ld", contentLength_);
          rv = onMessageHeader(name_, value_);
        } else if (iequals(name_, "Transfer-Encoding")) {
          if (iequals(value_, "chunked")) {
            chunked_ = true;
            rv = true;  // do not pass header to the upper layer if we've
                        // processed it
          } else {
            rv = onMessageHeader(name_, value_);
          }
        } else {
          rv = onMessageHeader(name_, value_);
        }

        name_.clear();
        value_.clear();

        // continue with the next header
        state_ = HEADER_NAME_BEGIN;

        if (!rv) {
          goto done;
        }
        break;
      }
      case HEADER_END_LF:
        if (*i == LF) {
          if (isContentExpected())
            state_ = CONTENT_BEGIN;
          else
            state_ = MESSAGE_BEGIN;

          ++*nparsed;
          ++i;

          if (!onMessageHeaderEnd()) {
            TRACE(2,
                  "messageHeaderEnd returned false. returning `Aborted`-state");
            goto done;
          }

          if (!isContentExpected() && !onMessageEnd()) {
            goto done;
          }
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case CONTENT_BEGIN:
        if (chunked_)
          state_ = CONTENT_CHUNK_SIZE_BEGIN;
        else if (contentLength_ >= 0)
          state_ = CONTENT;
        else
          state_ = CONTENT_ENDLESS;
        break;
      case CONTENT_ENDLESS: {
        // body w/o content-length (allowed in simple MESSAGE types only)
        BufferRef c(chunk.ref(*nparsed - initialOutOffset));

        // TRACE(2, "prepared content-chunk (%ld bytes): %s", c.size(),
        // c.str().c_str());

        *nparsed += c.size();
        i += c.size();

        bool rv = onMessageContent(c);

        if (!rv) goto done;

        break;
      }
      case CONTENT: {
        // fixed size content length
        std::size_t offset = *nparsed - initialOutOffset;
        std::size_t chunkSize = std::min(static_cast<size_t>(contentLength_),
                                         chunk.size() - offset);

        contentLength_ -= chunkSize;
        *nparsed += chunkSize;
        i += chunkSize;

        bool rv = onMessageContent(chunk.ref(offset, chunkSize));

        if (contentLength_ == 0) state_ = MESSAGE_BEGIN;

        if (!rv) goto done;

        if (state_ == MESSAGE_BEGIN && !onMessageEnd()) goto done;

        break;
      }
      case CONTENT_CHUNK_SIZE_BEGIN:
        if (!std::isxdigit(*i)) {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
          break;
        }
        state_ = CONTENT_CHUNK_SIZE;
        contentLength_ = 0;
      /* fall through */
      case CONTENT_CHUNK_SIZE:
        if (*i == CR) {
          state_ = CONTENT_CHUNK_LF1;
          ++*nparsed;
          ++i;
        } else if (*i >= '0' && *i <= '9') {
          contentLength_ = contentLength_ * 16 + *i - '0';
          ++*nparsed;
          ++i;
        } else if (*i >= 'a' && *i <= 'f') {
          contentLength_ = contentLength_ * 16 + 10 + *i - 'a';
          ++*nparsed;
          ++i;
        } else if (*i >= 'A' && *i <= 'F') {
          contentLength_ = contentLength_ * 16 + 10 + *i - 'A';
          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case CONTENT_CHUNK_LF1:
        if (*i != LF) {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          // TRACE(2, "content_length: %ld", contentLength_);
          if (contentLength_ != 0)
            state_ = CONTENT_CHUNK_BODY;
          else
            state_ = CONTENT_CHUNK_CR3;

          ++*nparsed;
          ++i;
        }
        break;
      case CONTENT_CHUNK_BODY:
        if (contentLength_) {
          std::size_t offset = *nparsed - initialOutOffset;
          std::size_t chunkSize = std::min(static_cast<size_t>(contentLength_),
                                           chunk.size() - offset);
          contentLength_ -= chunkSize;
          *nparsed += chunkSize;
          i += chunkSize;

          bool rv = onMessageContent(chunk.ref(offset, chunkSize));

          if (!rv) {
            goto done;
          }
        } else if (*i == CR) {
          state_ = CONTENT_CHUNK_LF2;
          ++*nparsed;
          ++i;
        } else {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        }
        break;
      case CONTENT_CHUNK_LF2:
        if (*i != LF) {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          state_ = CONTENT_CHUNK_SIZE;
          ++*nparsed;
          ++i;
        }
        break;
      case CONTENT_CHUNK_CR3:
        if (*i != CR) {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          state_ = CONTENT_CHUNK_LF3;
          ++*nparsed;
          ++i;
        }
        break;
      case CONTENT_CHUNK_LF3:
        if (*i != LF) {
          onProtocolError(HttpStatus::BadRequest);
          state_ = PROTOCOL_ERROR;
        } else {
          ++*nparsed;
          ++i;

          state_ = MESSAGE_BEGIN;

          if (!onMessageEnd()) goto done;
        }
        break;
      case PROTOCOL_ERROR:
        goto done;
      default:
#if !defined(NDEBUG)
        TRACE(1, "parse: unknown state %i", state_);
        if (std::isprint(*i)) {
          TRACE(1, "parse: internal error at nparsed: %ld, character: '%c'",
                *nparsed, *i);
        } else {
          TRACE(1, "parse: internal error at nparsed: %ld, character: 0x%02X",
                *nparsed, *i);
        }
        logDebug("HttpParser", "%s", chunk.hexdump().c_str());
#endif
        goto done;
    }
  }
  // we've reached the end of the chunk

  if (state_ == CONTENT_BEGIN) {
    // we've just parsed all headers but no body yet.

    if (contentLength_ < 0 && !chunked_ && mode_ != MESSAGE) {
      // and there's no body to come

      // subsequent calls to process() parse next request(s).
      state_ = MESSAGE_BEGIN;

      if (!onMessageEnd()) goto done;
    }
  }

done:
  return *nparsed - initialOutOffset;
}

void HttpParser::reset() {
  //.
  state_ = MESSAGE_BEGIN;
}

inline bool HttpParser::isChar(char value) {
  return static_cast<unsigned>(value) <= 127;
}

inline bool HttpParser::isControl(char value) {
  return (value >= 0 && value <= 31) || value == 127;
}

inline bool HttpParser::isSeparator(char value) {
  switch (value) {
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
    case '{':
    case '}':
    case SP:
    case HT:
      return true;
    default:
      return false;
  }
}

inline bool HttpParser::isToken(char value) {
  return isChar(value) && !(isControl(value) || isSeparator(value));
}

inline bool HttpParser::isText(char value) {
  // TEXT = <any OCTET except CTLs but including LWS>
  return !isControl(value) || value == SP || value == HT;
}

inline HttpVersion make_version(int versionMajor, int versionMinor) {
  if (versionMajor == 0) {
    if (versionMinor == 9)
      return HttpVersion::VERSION_0_9;
    else
      return HttpVersion::UNKNOWN;
  }

  if (versionMajor == 1) {
    if (versionMinor == 0)
      return HttpVersion::VERSION_1_0;
    else if (versionMinor == 1)
      return HttpVersion::VERSION_1_1;
  }

  return HttpVersion::UNKNOWN;
}

bool HttpParser::onMessageBegin(const BufferRef& method,
                                       const BufferRef& entity,
                                       int versionMajor, int versionMinor) {
  HttpVersion version = make_version(versionMajor, versionMinor);

  if (version == HttpVersion::UNKNOWN) {
    onProtocolError(HttpStatus::HttpVersionNotSupported);
    return false;
  }

  if (!listener_)
    return true;

  return listener_->onMessageBegin(method, entity, version);
}

bool HttpParser::onMessageBegin(int versionMajor, int versionMinor,
                                int status, const BufferRef& text) {
  HttpVersion version = make_version(versionMajor, versionMinor);
  if (version == HttpVersion::UNKNOWN) {
    onProtocolError(HttpStatus::HttpVersionNotSupported);
    return false;
  }

  if (!listener_)
    return true;

  return listener_->onMessageBegin(version,
                                   static_cast<HttpStatus>(code_), text);
}

bool HttpParser::onMessageBegin() {
  return listener_ ? listener_->onMessageBegin() : true;
}

bool HttpParser::onMessageHeader(const BufferRef& name,
                                        const BufferRef& value) {
  return listener_ ? listener_->onMessageHeader(name, value) : true;
}

bool HttpParser::onMessageHeaderEnd() {
  return listener_ ? listener_->onMessageHeaderEnd() : true;
}

bool HttpParser::onMessageContent(const BufferRef& chunk) {
  return listener_ ? listener_->onMessageContent(chunk) : true;
}

bool HttpParser::onMessageEnd() {
  return listener_ ? listener_->onMessageEnd() : true;
}

void HttpParser::onProtocolError(HttpStatus code, const std::string& message) {
  if (listener_) {
    listener_->onProtocolError(code, message);
  }
}

}  // namespace http1
}  // namespace xzero
