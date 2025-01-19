/**
   MppHTTPClient.cpp for MPP

   Created on: 02.11.2015

   Copyright (c) 2015 Markus Sattler. All rights reserved.
   This file is part of the ESP8266HTTPClient for Arduino.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    *
Author: MikeP  , adopted for ESP32 Ethernet by SergeyS
For devices managed by AutomationManager (AM)
 *

*/

#include <Arduino.h>
#include <Network.h>
#include <StreamString.h>
#include <base64.h>

#include "Mpp32HTTPClient.h"

unsigned MppHTTPClient::clientCount = 0;
MppHTTPClient** MppHTTPClient::clients = NULL;

/**
   converts error code to String
   @param error int
   @return String
*/
static String errorToString(int error) {
  switch (error) {
    case HTTPC_ERROR_CONNECTION_REFUSED:
      return F("connection refused");
    case HTTPC_ERROR_SEND_HEADER_FAILED:
      return F("send header failed");
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED:
      return F("send payload failed");
    case HTTPC_ERROR_NOT_CONNECTED:
      return F("not connected");
    case HTTPC_ERROR_CONNECTION_LOST:
      return F("connection lost");
    case HTTPC_ERROR_NO_STREAM:
      return F("no stream");
    case HTTPC_ERROR_NO_HTTP_SERVER:
      return F("no HTTP server");
    case HTTPC_ERROR_TOO_LESS_RAM:
      return F("not enough ram");
    case HTTPC_ERROR_ENCODING:
      return F("Transfer-Encoding not supported");
    case HTTPC_ERROR_STREAM_WRITE:
      return F("Stream write error");
    case HTTPC_ERROR_READ_TIMEOUT:
      return F("read timeout");
    case HTTPC_ERROR_CONNECT_TIMEOUT:
      return F("connect timeout");
    default:
      return F(""); // mpp
  }
}

String responseToString(int httpCode) {
  String result(httpCode);
  result += " " + errorToString(httpCode);
  result.trim();
  return result;
}

class TransportTraits {
  public:
    virtual ~TransportTraits() {
    }

    virtual std::unique_ptr<NetworkClient> create() {
      return std::unique_ptr<NetworkClient>(new NetworkClient()); // @suppress("Abstract class cannot be instantiated")
    }

    virtual bool verify(NetworkClient& client, const char* host) {
      (void) client;
      (void) host;
      return true;
    }
};

/**
   constructor
*/
MppHTTPClient::MppHTTPClient() {
}

/**
   destructor
*/
MppHTTPClient::~MppHTTPClient() {
  if (_tcp) {
    _tcp->stop();
  }
  if (_currentHeaders) {
    delete[] _currentHeaders;
  }
}

void MppHTTPClient::clear() {
  _size = -1;
  _headers = "";
  _httpResponse = 0;
  _lastDataTime = 0;
  _connectStartTime = 0;
  _hasConnected = false;
  _sentRequest = false;
  _type = "";
}

/**
   parsing the url for all needed parameters
   @param url String
*/
bool MppHTTPClient::begin(String url) {
  _transportTraits.reset(nullptr);
  _port = 80;
  if (!beginInternal(url, "http")) {
    return false;
  }
  _transportTraits = TransportTraitsPtr(new TransportTraits());
  return true;
}

bool MppHTTPClient::beginInternal(String url, const char* expectedProtocol) {
  DEBUG_HTTPCLIENT("[HTTP-Client][begin] url: %s\n", url.c_str());
  clear();

  // check for : (http: or https:
  int index = url.indexOf(':');
  if (index < 0) {
    DEBUG_HTTPCLIENT("[HTTP-Client][begin] failed to parse protocol\n");
    return false;
  }

  _protocol = url.substring(0, index);
  url.remove(0, (index + 3)); // remove http:// or https://

  index = url.indexOf('/');
  String host = url.substring(0, index);
  url.remove(0, index); // remove host part

  // get Authorization
  index = host.indexOf('@');
  if (index >= 0) {
    // auth info
    String auth = host.substring(0, index);
    host.remove(0, index + 1); // remove auth part including @
    _base64Authorization = base64::encode(auth);
  }

  // get port
  index = host.indexOf(':');
  if (index >= 0) {
    _host = host.substring(0, index); // hostname
    host.remove(0, (index + 1)); // remove hostname + :
    _port = host.toInt(); // get port
  } else {
    _host = host;
  }
  _uri = url;

  if (_protocol != expectedProtocol) {
    DEBUG_HTTPCLIENT(
      "[HTTP-Client][begin] unexpected protocol: %s, expected %s\n",
      _protocol.c_str(), expectedProtocol);
    return false;
  } DEBUG_HTTPCLIENT("[HTTP-Client][begin] host: %s port: %d url: %s\n",
                     _host.c_str(), _port, _uri.c_str());
  return true;
}

bool MppHTTPClient::begin(String host, uint16_t port, String uri) {
  clear();
  _host = host;
  _port = port;
  _uri = uri;
  _transportTraits = TransportTraitsPtr(new TransportTraits());
  DEBUG_HTTPCLIENT("[HTTP-Client][begin] host: %s port: %d uri: %s\n",
                   host.c_str(), port, uri.c_str());
  return true;
}

/**
   connected
   @return connected status
*/
bool MppHTTPClient::connected() {
  if (_tcp) {
    return (_tcp->connected() || (_tcp->available() > 0));
  }
  return false;
}

/**
   try to reuse the connection to the server
   keep-alive
   @param reuse bool
*/
void MppHTTPClient::setReuse(bool reuse) {
  _reuse = reuse;
}

/**
   set User Agent
   @param userAgent const char
*/
void MppHTTPClient::setUserAgent(const String& userAgent) {
  _userAgent = userAgent;
}

/**
   set the Authorizatio for the http request
   @param user const char
   @param password const char
*/
void MppHTTPClient::setAuthorization(const char * user, const char * password) {
  if (user && password) {
    String auth = user;
    auth += ":";
    auth += password;
    _base64Authorization = base64::encode(auth);
  }
}

/**
   set the Authorizatio for the http request
   @param auth const char * base64
*/
void MppHTTPClient::setAuthorization(const char * auth) {
  if (auth) {
    _base64Authorization = auth;
  }
}

/**
   set the timeout for the TCP connection
   @param timeout unsigned int
*/
void MppHTTPClient::setTimeout(uint16_t timeout) {
  _tcpTimeout = timeout;
  if (connected()) {
    _tcp->setTimeout(timeout);
  }
}

/**
   use HTTP1.0
   @param timeout
*/
void MppHTTPClient::useHTTP10(bool useHTTP10) {
  _useHTTP10 = useHTTP10;
}

/**
   send a GET request
   @return http code
*/
void MppHTTPClient::GET() {
  sendRequest("GET", "");
}

void MppHTTPClient::POST(String payload) {
  sendRequest("POST", payload);
}

void MppHTTPClient::PUT(String payload) {
  sendRequest("PUT", payload);
}

/**
   sendRequest
   @param type const char *     "GET", "POST", ....
   @param payload uint8_t *     data for the message body if null not send
   @param size size_t           size for the message body if 0 not send
   @return -1 if no info or > 0 when Content-Length is set by server
*/
void MppHTTPClient::sendRequest(const char * type, String payload) {

  if (payload.length() > 0) {
    addHeader(F("Content-Length"), String(payload.length()));
  }

  _type = String(type);
  _payload = payload;

}

/**
   size of message body / payload
   @return -1 if no info or > 0 when Content-Length is set by server
*/
int MppHTTPClient::getSize(void) {
  return _size;
}

/**
   adds Header to the request
   @param name
   @param value
   @param first
*/
void MppHTTPClient::addHeader(const String& name, const String& value,
                              bool first, bool replace) {
  // not allow set of Header handled by code
  if (!name.equalsIgnoreCase(F("Connection"))
      && !name.equalsIgnoreCase(F("User-Agent"))
      && !name.equalsIgnoreCase(F("Host"))
      && !(name.equalsIgnoreCase(F("Authorization"))
           && _base64Authorization.length())) {

    String headerLine = name;
    headerLine += ": ";

    if (replace) {
      int headerStart = _headers.indexOf(headerLine);
      if (headerStart != -1) {
        int headerEnd = _headers.indexOf('\n', headerStart);
        _headers = _headers.substring(0, headerStart)
                   + _headers.substring(headerEnd + 1);
      }
    }

    headerLine += value;
    headerLine += "\r\n";
    if (first) {
      _headers = headerLine + _headers;
    } else {
      _headers += headerLine;
    }
  }

}

void MppHTTPClient::collectHeaders(const char* headerKeys[],
                                   const size_t headerKeysCount) {
  _headerKeysCount = headerKeysCount;
  if (_currentHeaders) {
    delete[] _currentHeaders;
  }
  _currentHeaders = new RequestArgument[_headerKeysCount];
  for (size_t i = 0; i < _headerKeysCount; i++) {
    _currentHeaders[i].key = headerKeys[i];
  }
}

String MppHTTPClient::header(const char* name) {
  for (size_t i = 0; i < _headerKeysCount; ++i) {
    if (_currentHeaders[i].key == name) {
      return _currentHeaders[i].value;
    }
  }
  return String();
}

String MppHTTPClient::header(size_t i) {
  if (i < _headerKeysCount) {
    return _currentHeaders[i].value;
  }
  return String();
}

String MppHTTPClient::headerName(size_t i) {
  if (i < _headerKeysCount) {
    return _currentHeaders[i].key;
  }
  return String();
}

int MppHTTPClient::headers() {
  return _headerKeysCount;
}

bool MppHTTPClient::hasHeader(const char* name) {
  for (size_t i = 0; i < _headerKeysCount; ++i) {
    if ((_currentHeaders[i].key == name)
        && (_currentHeaders[i].value.length() > 0)) {
      return true;
    }
  }
  return false;
}

/**
   init TCP connection and handle ssl verify if needed
   @return true if connection is ok
*/
bool MppHTTPClient::connect(void) {

  DEBUG_HTTPCLIENT("[HTTP-Client] connecting to %s:%u with connectTO=%d\n",
                   _host.c_str(), _port, _tcpTimeout);

  if (connected())
    return true;

  _tcp->setTimeout(_tcpTimeout);
  _tcp->setNoDelay(true);

  if (!_tcp->connect(_host.c_str(), _port)) {
    DEBUG_HTTPCLIENT("[HTTP-Client] failed connect to %s:%u\n",
                     _host.c_str(), _port);
    return false;
  }

  DEBUG_HTTPCLIENT("[HTTP-Client] connected to %s:%u\n", _host.c_str(),
                   _port);

  if (!_transportTraits->verify(*_tcp, _host.c_str())) {
    DEBUG_HTTPCLIENT("[HTTP-Client] transport level verify failed\n");
    _tcp->stop();
    return false;
  }

  _tcp->setNoDelay(true);

  _hasConnected = connected();

  return _hasConnected;
}

/**
   sends HTTP request header
   @param type (GET, POST, ...)
   @return status
*/
bool MppHTTPClient::sendHeader(String type) {
  if (!connected()) {
    return false;
  }

  String header = type + " " + (_uri.length() ? _uri : F("/")) + F(" HTTP/1.");

  if (_useHTTP10) {
    header += "0";
  } else {
    header += "1";
  }

  header += String(F("\r\nHost: ")) + _host;
  if (_port != 80 && _port != 443) {
    header += ':';
    header += String(_port);
  }
  header +=
    String(F("\r\nUser-Agent: ")) + _userAgent + F("\r\nConnection: ");

  if (_reuse) {
    header += F("keep-alive");
  } else {
    header += F("close");
  }
  header += "\r\n";

  if (!_useHTTP10) {
    header += F("Accept-Encoding: identity;q=1,chunked;q=0.1,*;q=0\r\n");
  }

  if (_base64Authorization.length()) {
    _base64Authorization.replace("\n", "");
    header += F("Authorization: Basic ");
    header += _base64Authorization;
    header += "\r\n";
  }

  header += _headers + "\r\n";

  DEBUG_HTTPCLIENT("[HTTP-Client] sending request header\n-----\n%s-----\n",
                   header.c_str());

  return (_tcp->write((const uint8_t *) header.c_str(), header.length())
          == header.length());
}

/**
   return all payload as String (may need lot of ram or trigger out of memory!)
   @return String
*/
String MppHTTPClient::getString(void) {
  StreamString sstring; // @suppress("Abstract class cannot be instantiated")

  if (_size) {
    // try to reserve needed memmory
    if (!sstring.reserve((_size + 1))) {
      DEBUG_HTTPCLIENT(
        "[HTTP-Client][getString] not enough memory to reserve a string! need: %d\n",
        (_size + 1));
      return "";
    }
  }

  writeToStream(&sstring);
  return sstring;
}

/**
   write all  message body / payload to Stream
   @param stream Stream
   @return bytes written ( negative values are error codes )
*/
int MppHTTPClient::writeToStream(Stream * stream) {

  if (!stream) {
    reportError(HTTPC_ERROR_NO_STREAM);
  }

  if (!connected()) {
    reportError(HTTPC_ERROR_NOT_CONNECTED);
  }

  // get length of document (is -1 when Server sends no Content-Length header)
  int len = _size;
  int ret = 0;

  if (_transferEncoding == HTTPC_TE_IDENTITY) {
    ret = writeToStreamDataBlock(stream, len);

    // have we an error?
    if (ret < 0) {
      reportError(ret);
    }
  } else if (_transferEncoding == HTTPC_TE_CHUNKED) {
    int size = 0;
    while (1) {
      if (!connected()) {
        reportError(HTTPC_ERROR_CONNECTION_LOST);
      }
      String chunkHeader = _tcp->readStringUntil('\n');

      if (chunkHeader.length() <= 0) {
        reportError(HTTPC_ERROR_READ_TIMEOUT);
      }

      chunkHeader.trim(); // remove \r

      // read size of chunk
      len = (uint32_t) strtol((const char *) chunkHeader.c_str(), NULL,
                              16);
      size += len;
      DEBUG_HTTPCLIENT("[HTTP-Client] read chunk len: %d\n", len);

      // data left?
      if (len > 0) {
        int r = writeToStreamDataBlock(stream, len);
        if (r < 0) {
          // error in writeToStreamDataBlock
          reportError(r);
        }
        ret += r;
      } else {

        // if no length Header use global chunk size
        if (_size <= 0) {
          _size = size;
        }

        // check if we have write all data out
        if (ret != _size) {
          reportError(HTTPC_ERROR_STREAM_WRITE);
        }
        break;
      }

      // read trailing \r\n at the end of the chunk
      char buf[2];
      auto trailing_seq_len = _tcp->readBytes((uint8_t*) buf, 2);
      if (trailing_seq_len != 2 || buf[0] != '\r' || buf[1] != '\n') {
        reportError(HTTPC_ERROR_READ_TIMEOUT);
      }

      delay(0);
    }
  } else {
    reportError(HTTPC_ERROR_ENCODING);
  }
  return ret;
}

/**
   write one Data Block to Stream
   @param stream Stream
   @param size int
   @return < 0 = error >= 0 = size written
*/
int MppHTTPClient::writeToStreamDataBlock(Stream * stream, int size) {
  int buff_size = HTTP_TCP_BUFFER_SIZE;
  int len = size;
  int bytesWritten = 0;

  // if possible create smaller buffer then HTTP_TCP_BUFFER_SIZE
  if ((len > 0) && (len < HTTP_TCP_BUFFER_SIZE)) {
    buff_size = len;
  }

  // create buffer for read
  uint8_t * buff = (uint8_t *) malloc(buff_size);

  if (buff) {
    // read all data from server
    while (connected() && (len > 0 || len == -1)) {

      // get available data size
      size_t sizeAvailable = _tcp->available();

      if (sizeAvailable) {

        int readBytes = sizeAvailable;

        // read only the asked bytes
        if (len > 0 && readBytes > len) {
          readBytes = len;
        }

        // not read more the buffer can handle
        if (readBytes > buff_size) {
          readBytes = buff_size;
        }

        // read data
        int bytesRead = _tcp->readBytes(buff, readBytes);

        // write it to Stream
        int bytesWrite = stream->write(buff, bytesRead);
        bytesWritten += bytesWrite;

        // are all Bytes a writen to stream ?
        if (bytesWrite != bytesRead) {
          DEBUG_HTTPCLIENT(
            "[HTTP-Client][writeToStream] short write asked for %d but got %d retry...\n",
            bytesRead, bytesWrite);

          // check for write error
          if (stream->getWriteError()) {
            DEBUG_HTTPCLIENT(
              "[HTTP-Client][writeToStreamDataBlock] stream write error %d\n",
              stream->getWriteError());

            //reset write error for retry
            stream->clearWriteError();
          }

          // some time for the stream
          delay(1);

          int leftBytes = (readBytes - bytesWrite);

          // retry to send the missed bytes
          bytesWrite = stream->write((buff + bytesWrite), leftBytes);
          bytesWritten += bytesWrite;

          if (bytesWrite != leftBytes) {
            // failed again
            DEBUG_HTTPCLIENT(
              "[HTTP-Client][writeToStream] short write asked for %d but got %d failed.\n",
              leftBytes, bytesWrite);
            free(buff);
            return HTTPC_ERROR_STREAM_WRITE;
          }
        }

        // check for write error
        if (stream->getWriteError()) {
          DEBUG_HTTPCLIENT(
            "[HTTP-Client][writeToStreamDataBlock] stream write error %d\n",
            stream->getWriteError());
          free(buff);
          return HTTPC_ERROR_STREAM_WRITE;
        }

        // count bytes to read left
        if (len > 0) {
          len -= readBytes;
        }

        delay(0);
      } else {
        delay(1);
      }
    }

    free(buff);

    DEBUG_HTTPCLIENT(
      "[HTTP-Client][writeToStreamDataBlock] connection closed or file end (written: %d).\n",
      bytesWritten);

    if ((size > 0) && (size != bytesWritten)) {
      DEBUG_HTTPCLIENT(
        "[HTTP-Client][writeToStreamDataBlock] bytesWritten %d and size %d mismatch!.\n",
        bytesWritten, size);
      return HTTPC_ERROR_STREAM_WRITE;
    }

  } else {
    DEBUG_HTTPCLIENT(
      "[HTTP-Client][writeToStreamDataBlock] too less ram! need %d\n",
      HTTP_TCP_BUFFER_SIZE);
    return HTTPC_ERROR_TOO_LESS_RAM;
  }

  return bytesWritten;
}

/**
   called to handle error return, may disconnect the connection if still exists
   @param error
   @return error
*/
void MppHTTPClient::reportError(int error) {
  DEBUG_HTTPCLIENT("[HTTP-Client][returnError] error(%d): %s\n", error,
                   errorToString(error).c_str());
  if (resultHandler)
    resultHandler(error, this);
  _active = false;
}

void MppHTTPClient::handleClients() {
  MppHTTPClient** current = clients; // need to cache it in case of delete
  int count = clientCount;
  for (int i = 0; i < count && current != NULL; i++) {
    MppHTTPClient* client = current[i];
    if (client != NULL) {
      if (client->_active) {
        int result = client->handleClient();
        DEBUG_HTTPCLIENT(
          "[HTTP-Client] handleClients %p for %s result %d\n", client,
          client->_host.c_str(), result);
        if (result) {
          //					Serial.printf("HTTPClient result %s from '%s' at %s\n",
          //							responseToString(result).c_str(),
          //							client->_uri.c_str(), client->_host.c_str());
          if (client->resultHandler != NULL)
            client->resultHandler(result, client);
          // try to free it up now
          if (client->connected()) {
            DEBUG_HTTPCLIENT("[HTTP-Client][returnError] tcp stop\n");
            client->_tcp->stop();
          }
          // memory will be released next loop
          client->_active = false;
        }
      } else
        freeClient(client);
    }
  }
}

void MppHTTPClient::showClientStatus() {
  Serial.printf("[HTTP-Client] showClientStatus %d clients in %p.\n",
                clientCount, clients);
  MppHTTPClient** current = clients; // need to cache it in case of delete
  for (unsigned i = 0; i < clientCount; i++) {
    MppHTTPClient* client = current[i];
    if (client != NULL)
      Serial.printf(
        "[HTTP-Client] showClientStatus %d %p for %s connected=%d time=%lu _connect=%lu _last=%lu\n",
        i, client, client->_host.c_str(), client->connected(),
        millis(), client->_connectStartTime, client->_lastDataTime);
    else
      Serial.printf("[HTTP-Client] showClientStatus %d is NULL\n", i);
  }
}

MppHTTPClient* MppHTTPClient::allocateClient(
  std::function<void(int, MppHTTPClient*)> handleResult) {
  MppHTTPClient* client = new MppHTTPClient();
  client->resultHandler = handleResult;
  client->_active = true;
  ++clientCount;
  clients = (MppHTTPClient**) realloc(clients,
                                      clientCount * sizeof(MppHTTPClient*));
  clients[clientCount - 1] = client;
  return client;
}

void MppHTTPClient::freeClient(MppHTTPClient* client) {
  if (clients != NULL) {
    unsigned count = 0;
    for (unsigned i = 0; i < clientCount; i++)
      // see if it's contained here
      if (clients[i] == client) {
        clients[i] = NULL;
        client->_active = false;
        // make sure nothing is still connected
        if (client->connected()) {
          DEBUG_HTTPCLIENT("[HTTP-Client][returnError] tcp stop\n");
          client->_tcp->stop();
        }
        delete client;
      } else if (clients[i] != NULL)
        ++count;
    if (count == 0) { // if none left...
      free(clients);
      clients = NULL;
      clientCount = 0;
    }
  }
}

/**
   reads the response from the server
   MPP - this is the major function changed to make things async
   @return int http code
*/
int MppHTTPClient::handleClient() {

  DEBUG_HTTPCLIENT(
    "[HTTP-Client] handleClient %p for %s connected=%d time=%lu _connect=%lu _last=%lu...\n",
    this, _host.c_str(), connected(), millis(), _connectStartTime,
    _lastDataTime);

  if (!_active)
    return 0;

  if (_type.length() == 0)
    return 0; // request not ready to be sent yet

  String transferEncoding;
  _size = -1;
  _transferEncoding = HTTPC_TE_IDENTITY;

  if (_connectStartTime == 0) {
    if (!_transportTraits) {
      DEBUG_HTTPCLIENT(
        "[HTTP-Client] connect: HTTPClient::begin was not called or returned error\n");
      return HTTPC_ERROR_NO_BEGIN;
    }
    _tcp = _transportTraits->create();
    _connectStartTime = millis();
  }

  if (connected()) {

    if (_lastDataTime == 0)
      _lastDataTime = millis();

    if (!_sentRequest) {
      // send Header
      if (!sendHeader(_type)) {
        return HTTPC_ERROR_SEND_HEADER_FAILED;
      }

      // send Payload if needed
      if (_payload.length() > 0) {
        if (_tcp->write(_payload.c_str(), _payload.length())
            != _payload.length()) {
          return HTTPC_ERROR_SEND_PAYLOAD_FAILED;
        }
      }
      _sentRequest = true;
    }

    while (_tcp->available() > 0) {

      DEBUG_HTTPCLIENT("[HTTP-Client] handleClient %p %d bytes from %s\n", this,
                       _tcp->available(), _host.c_str());

      String headerLine = _tcp->readStringUntil('\n');
      headerLine.trim(); // remove \r

      _lastDataTime = millis();

      DEBUG_HTTPCLIENT("[HTTP-Client][handleHeaderResponse] RX: '%s'\n",
                       headerLine.c_str());

      if (headerLine.startsWith("HTTP/1.")) {
        _httpResponse = headerLine.substring(9,
                                             headerLine.indexOf(' ', 9)).toInt();
      } else if (headerLine.indexOf(':')) {
        String headerName = headerLine.substring(0,
                            headerLine.indexOf(':'));
        String headerValue = headerLine.substring(
                               headerLine.indexOf(':') + 1);
        headerValue.trim();

        if (headerName.equalsIgnoreCase("Content-Length")) {
          _size = headerValue.toInt();
        }

        if (headerName.equalsIgnoreCase("Connection")) {
          _canReuse = headerValue.equalsIgnoreCase("keep-alive");
        }

        if (headerName.equalsIgnoreCase("Transfer-Encoding")) {
          transferEncoding = headerValue;
        }

        for (size_t i = 0; i < _headerKeysCount; i++) {
          if (_currentHeaders[i].key.equalsIgnoreCase(headerName)) {
            _currentHeaders[i].value = headerValue;
            break;
          }
        }
      }

      if (headerLine == "") { // end of headers
        DEBUG_HTTPCLIENT(
          "[HTTP-Client][handleHeaderResponse] code: %d\n",
          _httpResponse);

        if (_size > 0) {
          DEBUG_HTTPCLIENT(
            "[HTTP-Client][handleHeaderResponse] size: %d\n",
            _size);
        }

        if (transferEncoding.length() > 0) {
          DEBUG_HTTPCLIENT(
            "[HTTP-Client][handleHeaderResponse] Transfer-Encoding: %s\n",
            transferEncoding.c_str());
          if (transferEncoding.equalsIgnoreCase("chunked")) {
            _transferEncoding = HTTPC_TE_CHUNKED;
          } else {
            return HTTPC_ERROR_ENCODING;
          }
        } else {
          _transferEncoding = HTTPC_TE_IDENTITY;
        }

        if (_httpResponse) {
          return _httpResponse;
        } else {
          DEBUG_HTTPCLIENT(
            "[HTTP-Client][handleHeaderResponse] Remote host is not an HTTP Server!");
          return HTTPC_ERROR_NO_HTTP_SERVER;
        }
      }

    }
    if ((millis() - _lastDataTime) > _tcpTimeout) // MPP - this timeout is specified in seconds
      return HTTPC_ERROR_READ_TIMEOUT;
  } else if (_hasConnected)
    return _httpResponse ? _httpResponse : HTTPC_ERROR_CONNECTION_LOST; // MPP return something if provided...
  else if (millis() - _connectStartTime > _tcpTimeout)
    return HTTPC_ERROR_CONNECT_TIMEOUT;
  else
    connect();

  return 0;
}
