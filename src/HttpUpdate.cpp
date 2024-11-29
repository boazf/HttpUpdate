/**
 *
 * @file HttpUpdate.cpp based om ESP8266HTTPUpdate.cpp
 * @date 16.10.2018
 * @author Markus Sattler
 *
 * Copyright (c) 2015 Markus Sattler. All rights reserved.
 * This file is part of the ESP32 Http Updater.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "HttpUpdate.h"
#include <StreamString.h>

#include <esp_partition.h>
#include <esp_ota_ops.h>                // get running partition

// To do extern "C" uint32_t _SPIFFS_start;
// To do extern "C" uint32_t _SPIFFS_end;
/// HTTP codes see RFC7231

typedef enum {
    HTTP_CODE_CONTINUE = 100,
    HTTP_CODE_SWITCHING_PROTOCOLS = 101,
    HTTP_CODE_PROCESSING = 102,
    HTTP_CODE_OK = 200,
    HTTP_CODE_CREATED = 201,
    HTTP_CODE_ACCEPTED = 202,
    HTTP_CODE_NON_AUTHORITATIVE_INFORMATION = 203,
    HTTP_CODE_NO_CONTENT = 204,
    HTTP_CODE_RESET_CONTENT = 205,
    HTTP_CODE_PARTIAL_CONTENT = 206,
    HTTP_CODE_MULTI_STATUS = 207,
    HTTP_CODE_ALREADY_REPORTED = 208,
    HTTP_CODE_IM_USED = 226,
    HTTP_CODE_MULTIPLE_CHOICES = 300,
    HTTP_CODE_MOVED_PERMANENTLY = 301,
    HTTP_CODE_FOUND = 302,
    HTTP_CODE_SEE_OTHER = 303,
    HTTP_CODE_NOT_MODIFIED = 304,
    HTTP_CODE_USE_PROXY = 305,
    HTTP_CODE_TEMPORARY_REDIRECT = 307,
    HTTP_CODE_PERMANENT_REDIRECT = 308,
    HTTP_CODE_BAD_REQUEST = 400,
    HTTP_CODE_UNAUTHORIZED = 401,
    HTTP_CODE_PAYMENT_REQUIRED = 402,
    HTTP_CODE_FORBIDDEN = 403,
    HTTP_CODE_NOT_FOUND = 404,
    HTTP_CODE_METHOD_NOT_ALLOWED = 405,
    HTTP_CODE_NOT_ACCEPTABLE = 406,
    HTTP_CODE_PROXY_AUTHENTICATION_REQUIRED = 407,
    HTTP_CODE_REQUEST_TIMEOUT = 408,
    HTTP_CODE_CONFLICT = 409,
    HTTP_CODE_GONE = 410,
    HTTP_CODE_LENGTH_REQUIRED = 411,
    HTTP_CODE_PRECONDITION_FAILED = 412,
    HTTP_CODE_PAYLOAD_TOO_LARGE = 413,
    HTTP_CODE_URI_TOO_LONG = 414,
    HTTP_CODE_UNSUPPORTED_MEDIA_TYPE = 415,
    HTTP_CODE_RANGE_NOT_SATISFIABLE = 416,
    HTTP_CODE_EXPECTATION_FAILED = 417,
    HTTP_CODE_MISDIRECTED_REQUEST = 421,
    HTTP_CODE_UNPROCESSABLE_ENTITY = 422,
    HTTP_CODE_LOCKED = 423,
    HTTP_CODE_FAILED_DEPENDENCY = 424,
    HTTP_CODE_UPGRADE_REQUIRED = 426,
    HTTP_CODE_PRECONDITION_REQUIRED = 428,
    HTTP_CODE_TOO_MANY_REQUESTS = 429,
    HTTP_CODE_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
    HTTP_CODE_INTERNAL_SERVER_ERROR = 500,
    HTTP_CODE_NOT_IMPLEMENTED = 501,
    HTTP_CODE_BAD_GATEWAY = 502,
    HTTP_CODE_SERVICE_UNAVAILABLE = 503,
    HTTP_CODE_GATEWAY_TIMEOUT = 504,
    HTTP_CODE_HTTP_VERSION_NOT_SUPPORTED = 505,
    HTTP_CODE_VARIANT_ALSO_NEGOTIATES = 506,
    HTTP_CODE_INSUFFICIENT_STORAGE = 507,
    HTTP_CODE_LOOP_DETECTED = 508,
    HTTP_CODE_NOT_EXTENDED = 510,
    HTTP_CODE_NETWORK_AUTHENTICATION_REQUIRED = 511
} t_http_codes;

HttpUpdate::HttpUpdate(void)
        : _httpClientTimeout(8000), _ledPin(-1)
{
}

HttpUpdate::HttpUpdate(int httpClientTimeout)
        : _httpClientTimeout(httpClientTimeout), _ledPin(-1)
{
}

HttpUpdate::~HttpUpdate(void)
{
}

HttpUpdateResult HttpUpdate::update(Client& client, const String& url, const String& currentVersion)
{
    HttpClientEx http(client);

    if(!http.begin(url))
    {
        _setLastError(HTTP_ERROR_API);
        return HTTP_UPDATE_FAILED;
    }
    return handleUpdate(http, currentVersion, false);
}

HttpUpdateResult HttpUpdate::updateSpiffs(HttpClientEx& httpClient, const String& currentVersion)
{
    return handleUpdate(httpClient, currentVersion, true);
}

HttpUpdateResult HttpUpdate::updateSpiffs(Client& client, const String& url, const String& currentVersion)
{
    HttpClientEx http(client);
    if(!http.begin(url))
    {
        _setLastError(HTTP_ERROR_API);
        return HTTP_UPDATE_FAILED;
    }
    return handleUpdate(http, currentVersion, true);
}

HttpUpdateResult HttpUpdate::update(HttpClientEx& httpClient, const String& currentVersion)
{
    return handleUpdate(httpClient, currentVersion, false);
}

HttpUpdateResult HttpUpdate::update(Client& client, const String& host, uint16_t port, const String& uri,
        const String& currentVersion)
{
    HttpClientEx http(client);
    http.begin(host, port, uri);
    return handleUpdate(http, currentVersion, false);
}

/**
 * return error code as int
 * @return int error code
 */
int HttpUpdate::getLastError(void)
{
    return _lastError;
}

/**
 * return error code as String
 * @return String error
 */
String HttpUpdate::getLastErrorString(void)
{

    if(_lastError == 0) {
        return String(); // no error
    }

    // error from Update class
    if(_lastError > 0) {
        StreamString error;
        Update.printError(error);
        error.trim(); // remove line ending
        return String("Update error: ") + error;
    }

    // error from http client
    if(_lastError > -100) {
        return String("HTTP error: ") + HttpClientEx::errorToString(_lastError);
    }

    switch(_lastError) {
    case HTTP_UE_TOO_LESS_SPACE:
        return "Not Enough space";
    case HTTP_UE_SERVER_NOT_REPORT_SIZE:
        return "Server Did Not Report Size";
    case HTTP_UE_SERVER_FILE_NOT_FOUND:
        return "File Not Found (404)";
    case HTTP_UE_SERVER_FORBIDDEN:
        return "Forbidden (403)";
    case HTTP_UE_SERVER_WRONG_HTTP_CODE:
        return "Wrong HTTP Code";
    case HTTP_UE_SERVER_FAULTY_MD5:
        return "Wrong MD5";
    case HTTP_UE_BIN_VERIFY_HEADER_FAILED:
        return "Verify Bin Header Failed";
    case HTTP_UE_BIN_FOR_WRONG_FLASH:
        return "New Binary Does Not Fit Flash Size";
    case HTTP_UE_NO_PARTITION:
        return "Partition Could Not be Found";
    }

    return String();
}


String getSketchSHA256() {
  const size_t HASH_LEN = 32; // SHA-256 digest length

  uint8_t sha_256[HASH_LEN] = { 0 };

// get sha256 digest for running partition
  if(esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256) == 0) {
    char buffer[2 * HASH_LEN + 1];

    for(size_t index = 0; index < HASH_LEN; index++) {
      uint8_t nibble = (sha_256[index] & 0xf0) >> 4;
      buffer[2 * index] = nibble < 10 ? char(nibble + '0') : char(nibble - 10 + 'A');

      nibble = sha_256[index] & 0x0f;
      buffer[2 * index + 1] = nibble < 10 ? char(nibble + '0') : char(nibble - 10 + 'A');
    }

    buffer[2 * HASH_LEN] = '\0';

    return String(buffer);
  } else {

    return String();
  }
}

/**
 *
 * @param http HTTPClient *
 * @param currentVersion const char *
 * @return HttpUpdateResult
 */
HttpUpdateResult HttpUpdate::handleUpdate(HttpClientEx& http, const String& currentVersion, bool spiffs)
{

    HttpUpdateResult ret = HTTP_UPDATE_FAILED;

    // use HTTP/1.0 for update since the update handler not support any transfer Encoding
    //http.useHTTP10(true);
    http.setHttpResponseTimeout(_httpClientTimeout);
    http.beginRequest();
    int code = http.startRequest(HTTP_METHOD_GET, NULL);

    log_d("startRequest: %d\n", code);

    if(code != 0) {
        log_e("HTTP error: %d\n", code);
        _setLastError(code);
        return HTTP_UPDATE_FAILED;
    }

    http.sendAuthorizationHeader();
    http.sendHeader("Cache-Control", "no-cache");
    http.sendHeader("x-ESP32-free-space", ESP.getFreeSketchSpace());
    http.sendHeader("x-ESP32-sketch-size", ESP.getSketchSize());
    String sketchMD5 = ESP.getSketchMD5();
    log_d("Sketch MD5: %s\n", sketchMD5.c_str());
    if(sketchMD5.length() != 0) {
        http.sendHeader("x-ESP32-sketch-md5", sketchMD5.c_str());
    }
    // Add also a SHA256
    String sketchSHA256 = getSketchSHA256();
    if(sketchSHA256.length() != 0) {
      http.sendHeader("x-ESP32-sketch-sha256", sketchSHA256.c_str());
    }
    http.sendHeader("x-ESP32-chip-size", ESP.getFlashChipSize());
    http.sendHeader("x-ESP32-sdk-version", ESP.getSdkVersion());

    if(spiffs) {
        http.sendHeader("x-ESP32-mode", "spiffs");
    } else {
        http.sendHeader("x-ESP32-mode", "sketch");
    }
    if(currentVersion && currentVersion[0] != 0x00) {
        http.sendHeader("x-ESP32-version", currentVersion.c_str());
    }
    http.endRequest();

    code = http.responseStatusCode();

    HttpClientEx::Headers headers[] = { String("x-MD5") };
    http.collectHeaders(headers, sizeof(headers)/sizeof(*headers));
    _md5 = headers[0].value;
    if (_md5 && !_md5.isEmpty())
    {
        _md5.toLowerCase();
        log_d("x-MD5 = \"%s\"\n", _md5.c_str());
    }

    int len = http.contentLength();

    log_d("Header read fin.\n");
    log_d("Server header:\n");
    log_d(" - code: %d\n", code);
    log_d(" - len: %d\n", len);
    log_d("ESP32 info:\n");
    log_d(" - free Space: %d\n", ESP.getFreeSketchSpace());
    log_d(" - current Sketch Size: %d\n", ESP.getSketchSize());

    if(currentVersion && currentVersion[0] != 0x00) {
        log_d(" - current version: %s\n", currentVersion.c_str() );
    }


    switch(code) {
    case HTTP_CODE_OK:  ///< OK (Start Update)
        if(len > 0) {
            bool startUpdate = true;
            if(spiffs) {
                const esp_partition_t* _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
                if(!_partition){
                    _setLastError(HTTP_UE_NO_PARTITION);
                    return HTTP_UPDATE_FAILED;
                }

                if(len > _partition->size) {
                    log_e("spiffsSize to low (%d) needed: %d\n", _partition->size, len);
                    startUpdate = false;
                }
            } else {
                int sketchFreeSpace = ESP.getFreeSketchSpace();
                if(!sketchFreeSpace){
                    _setLastError(HTTP_UE_NO_PARTITION);
                    return HTTP_UPDATE_FAILED;
                }

                if(len > sketchFreeSpace) {
                    log_e("FreeSketchSpace to low (%d) needed: %d\n", sketchFreeSpace, len);
                    startUpdate = false;
                }
            }

            if(!startUpdate) {
                _setLastError(HTTP_UE_TOO_LESS_SPACE);
                ret = HTTP_UPDATE_FAILED;
            } else {
                // Warn main app we're starting up...
                if (_cbStart) {
                    _cbStart();
                }

                Client * tcp = &http;

// To do?                WiFiUDP::stopAll();
// To do?                WiFiClient::stopAllExcept(tcp);

                delay(100);

                int command;

                if(spiffs) {
                    command = U_SPIFFS;
                    log_d("runUpdate spiffs...\n");
                } else {
                    command = U_FLASH;
                    log_d("runUpdate flash...\n");
                }

                if(!spiffs) {
/* To do
                    uint8_t buf[4];
                    if(tcp->peekBytes(&buf[0], 4) != 4) {
                        log_e("peekBytes magic header failed\n");
                        _setLastError(HTTP_UE_BIN_VERIFY_HEADER_FAILED);
                        http.end();
                        return HTTP_UPDATE_FAILED;
                    }
*/

                    // check for valid first magic byte
//                    if(buf[0] != 0xE9) {
                    if(tcp->peek() != 0xE9) {
                        log_e("Magic header does not start with 0xE9\n");
                        _setLastError(HTTP_UE_BIN_VERIFY_HEADER_FAILED);
//                        http.end();
                        return HTTP_UPDATE_FAILED;

                    }
/* To do
                    uint32_t bin_flash_size = ESP.magicFlashChipSize((buf[3] & 0xf0) >> 4);

                    // check if new bin fits to SPI flash
                    if(bin_flash_size > ESP.getFlashChipRealSize()) {
                        log_e("New binary does not fit SPI Flash size\n");
                        _setLastError(HTTP_UE_BIN_FOR_WRONG_FLASH);
                        http.end();
                        return HTTP_UPDATE_FAILED;
                    }
*/
                }
                if(runUpdate(*tcp, len, _md5, command)) {
                    ret = HTTP_UPDATE_OK;
                    log_d("Update ok\n");
//                    http.end();
                    // Warn main app we're all done
                    if (_cbEnd) {
                        _cbEnd();
                    }

                    if(_rebootOnUpdate && !spiffs) {
                        ESP.restart();
                    }

                } else {
                    ret = HTTP_UPDATE_FAILED;
                    log_e("Update failed\n");
                }
            }
        } else {
            _setLastError(HTTP_UE_SERVER_NOT_REPORT_SIZE);
            ret = HTTP_UPDATE_FAILED;
            log_e("Content-Length was 0 or wasn't set by Server?!\n");
        }
        break;
    case HTTP_CODE_NOT_MODIFIED:
        ///< Not Modified (No updates)
        ret = HTTP_UPDATE_NO_UPDATES;
        break;
    case HTTP_CODE_NOT_FOUND:
        _setLastError(HTTP_UE_SERVER_FILE_NOT_FOUND);
        ret = HTTP_UPDATE_FAILED;
        break;
    case HTTP_CODE_FORBIDDEN:
        _setLastError(HTTP_UE_SERVER_FORBIDDEN);
        ret = HTTP_UPDATE_FAILED;
        break;
    default:
        _setLastError(HTTP_UE_SERVER_WRONG_HTTP_CODE);
        ret = HTTP_UPDATE_FAILED;
        log_e("HTTP Code is (%d)\n", code);
        break;
    }

//    http.end();
    return ret;
}

/**
 * write Update to flash
 * @param in Stream&
 * @param size uint32_t
 * @param md5 String
 * @return true if Update ok
 */
bool HttpUpdate::runUpdate(Stream& in, uint32_t size, String md5, int command)
{

    StreamString error;

    if (_cbProgress) {
        Update.onProgress(_cbProgress);
    }

    if(!Update.begin(size, command, _ledPin, _ledOn)) {
        _setLastError(Update.getError());
        Update.printError(error);
        error.trim(); // remove line ending
        log_e("Update.begin failed! (%s)\n", error.c_str());
        return false;
    }

    if (_cbProgress) {
        _cbProgress(0, size);
    }

    if(md5.length()) {
        if(!Update.setMD5(md5.c_str())) {
            _setLastError(HTTP_UE_SERVER_FAULTY_MD5);
            log_e("Update.setMD5 failed! (%s)\n", md5.c_str());
            return false;
        }
    }

// To do: the SHA256 could be checked if the server sends it

    if(Update.writeStream(in) != size) {
        _setLastError(Update.getError());
        Update.printError(error);
        error.trim(); // remove line ending
        log_e("Update.writeStream failed! (%s)\n", error.c_str());
        return false;
    }

    if (_cbProgress) {
        _cbProgress(size, size);
    }

    if(!Update.end()) {
        _setLastError(Update.getError());
        Update.printError(error);
        error.trim(); // remove line ending
        log_e("Update.end failed! (%s)\n", error.c_str());
        return false;
    }

    return true;
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_HttpUpdate)
HttpUpdate httpUpdate;
#endif
