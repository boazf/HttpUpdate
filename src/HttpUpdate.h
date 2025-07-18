/*
 * Modified version of HttpUpdate.h, originally based on ESP8266HTTPUpdate.h
 * 
 * Original Author: Markus Sattler
 * Original Copyright (c) 2015 Markus Sattler. All rights reserved.
 * 
 * This file is part of the ESP32 Http Updater and is licensed under the
 * GNU Lesser General Public License as published by the Free Software Foundation;
 * either version 2.1 of the License, or (at your option) any later version.
 * 
 * This modified version:
 * The original code required WiFiClient as a parameter. The modification in the code
 * is intended to use Client. This allows using WiFiClient or EthernetClient. 
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef ___Http_UPDATE_H___
#define ___Http_UPDATE_H___

#include <Arduino.h>
#include <HttpClientEx.h>
#include <Update.h>

/// note we use HTTP client errors too so we start at 100
#define HTTP_UE_TOO_LESS_SPACE              (-100)
#define HTTP_UE_SERVER_NOT_REPORT_SIZE      (-101)
#define HTTP_UE_SERVER_FILE_NOT_FOUND       (-102)
#define HTTP_UE_SERVER_FORBIDDEN            (-103)
#define HTTP_UE_SERVER_WRONG_HTTP_CODE      (-104)
#define HTTP_UE_SERVER_FAULTY_MD5           (-105)
#define HTTP_UE_BIN_VERIFY_HEADER_FAILED    (-106)
#define HTTP_UE_BIN_FOR_WRONG_FLASH         (-107)
#define HTTP_UE_NO_PARTITION                (-108)

enum HttpUpdateResult {
    HTTP_UPDATE_FAILED,
    HTTP_UPDATE_NO_UPDATES,
    HTTP_UPDATE_OK
};

typedef HttpUpdateResult t_httpUpdate_return; // backward compatibility

using HttpUpdateStartCB = std::function<void()>;
using HttpUpdateEndCB = std::function<void()>;
using HttpUpdateErrorCB = std::function<void(int)>;
using HttpUpdateProgressCB = std::function<void(int, int)>;

class HttpUpdate
{
public:
    HttpUpdate(void);
    HttpUpdate(int httpClientTimeout);
    ~HttpUpdate(void);

    void rebootOnUpdate(bool reboot)
    {
        _rebootOnUpdate = reboot;
    }
    
    // /**
    //   * set redirect follow mode. See `followRedirects_t` enum for avaliable modes.
    //   * @param follow
    //   */
    // void setFollowRedirects(followRedirects_t follow)
    // {
    //     _followRedirects = follow;
    // }

    void setLedPin(int ledPin = -1, uint8_t ledOn = HIGH)
    {
        _ledPin = ledPin;
        _ledOn = ledOn;
    }

    t_httpUpdate_return update(Client& client, const String& url, const String& currentVersion = "");

    t_httpUpdate_return update(Client& client, const String& host, uint16_t port, const String& uri = "/",
                               const String& currentVersion = "");

    t_httpUpdate_return updateSpiffs(Client& client, const String& url, const String& currentVersion = "");

    t_httpUpdate_return update(HttpClientEx& httpClient,
                               const String& currentVersion = "");

    t_httpUpdate_return updateSpiffs(HttpClientEx &httpClient, const String &currentVersion = "");

    // Notification callbacks
    void onStart(HttpUpdateStartCB cbOnStart)          { _cbStart = cbOnStart; }
    void onEnd(HttpUpdateEndCB cbOnEnd)                { _cbEnd = cbOnEnd; }
    void onError(HttpUpdateErrorCB cbOnError)          { _cbError = cbOnError; }
    void onProgress(HttpUpdateProgressCB cbOnProgress) { _cbProgress = cbOnProgress; }

    int getLastError(void);
    String getLastErrorString(void);

protected:
    t_httpUpdate_return handleUpdate(HttpClientEx& http, const String& currentVersion, bool spiffs = false);
    bool runUpdate(Stream& in, uint32_t size, String md5, int command = U_FLASH);

    // Set the error and potentially use a CB to notify the application
    void _setLastError(int err) {
        _lastError = err;
        if (_cbError) {
            _cbError(err);
        }
    }
    int _lastError;
    bool _rebootOnUpdate = true;
    String _md5;
private:
    int _httpClientTimeout;
    // followRedirects_t _followRedirects;

    // Callbacks
    HttpUpdateStartCB    _cbStart;
    HttpUpdateEndCB      _cbEnd;
    HttpUpdateErrorCB    _cbError;
    HttpUpdateProgressCB _cbProgress;

    int _ledPin;
    uint8_t _ledOn;
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_HTTPUPDATE)
extern HttpUpdate httpUpdate;
#endif

#endif /* ___HTTP_UPDATE_H___ */
