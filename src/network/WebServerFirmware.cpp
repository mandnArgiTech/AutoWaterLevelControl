/**
 * @file WebServerFirmware.cpp
 * @brief Web OTA upload handlers (multipart POST /api/update)
 */
#include "WebServer.h"
#include <Updater.h>
#include "../utils/Log.h"
#include "../utils/BootDiagnostics.h"

void WebServerManager::handleFirmwareUpload() {
    HTTPUpload& upload = _server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        _firmwareUploadOk = false;
        if (_otaPrepare) {
            _otaPrepare();
        }
        FLM_LOG_INFO("WebOTA", "upload start: %s", upload.filename.c_str());
        uint32_t maxSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSpace)) {
            Update.printError(Serial);
        } else {
            FLM_LOG_DEBUG("WebOTA", "free heap: %u", (unsigned)ESP.getFreeHeap());
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
        yield();
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            _firmwareUploadOk = true;
            FLM_LOG_INFO("WebOTA", "done %u bytes — reboot", (unsigned)upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.end(false);
        if (_otaRestore) {
            _otaRestore();
        }
        FLM_LOG_WARN("WebOTA", "upload aborted");
    }
}

void WebServerManager::handleFirmwareUploadComplete() {
    _requestCount++;
    addCorsHeaders();
    if (_firmwareUploadOk) {
        _server.send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting\"}");
        BootDiagnostics::getInstance().markIntentionalRestart("web_ota");
        delay(50);
        ESP.restart();
    } else {
        if (_otaRestore) {
            _otaRestore();
        }
        sendApiFailure(500, "FIRMWARE_UPDATE_FAILED", "Firmware update failed");
    }
}
