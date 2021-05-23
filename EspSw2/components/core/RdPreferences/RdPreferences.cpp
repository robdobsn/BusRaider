// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "RdPreferences.h"
#include "nvs.h"
#include <WString.h>
#include "Logger.h"

static const char *MODULE_PREFIX = "RdPreferences";

const char *nvs_errors[] = {"OTHER", "NOT_INITIALIZED", "NOT_FOUND", "TYPE_MISMATCH", "READ_ONLY", "NOT_ENOUGH_SPACE", "INVALID_NAME", "INVALID_HANDLE", "REMOVE_FAILED", "KEY_TOO_LONG", "PAGE_FULL", "INVALID_STATE", "INVALID_LENGHT"};
#define nvs_error(e) (((e) > ESP_ERR_NVS_BASE) ? nvs_errors[(e) & ~(ESP_ERR_NVS_BASE)] : nvs_errors[0])

RdPreferences::RdPreferences()
    : _handle(0), _started(false), _readOnly(false)
{
}

RdPreferences::~RdPreferences()
{
    end();
}

bool RdPreferences::begin(const char *name, bool readOnly)
{
    if (_started)
    {
        return false;
    }
    _readOnly = readOnly;
    esp_err_t err = nvs_open(name, readOnly ? NVS_READONLY : NVS_READWRITE, &_handle);
    if (err)
    {
        LOG_V(MODULE_PREFIX, "nvs_open failed: %s", nvs_error(err));
        return false;
    }
    _started = true;
    return true;
}

void RdPreferences::end()
{
    if (!_started)
    {
        return;
    }
    nvs_close(_handle);
    _started = false;
}

/*
 * Clear all keys in opened preferences
 * */

bool RdPreferences::clear()
{
    if (!_started || _readOnly)
    {
        return false;
    }
    esp_err_t err = nvs_erase_all(_handle);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_erase_all fail: %s", nvs_error(err));
        return false;
    }
    return true;
}

/*
 * Remove a key
 * */

bool RdPreferences::remove(const char *key)
{
    if (!_started || !key || _readOnly)
    {
        return false;
    }
    esp_err_t err = nvs_erase_key(_handle, key);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_erase_key fail: %s %s", key, nvs_error(err));
        return false;
    }
    return true;
}

/*
 * Put a key value
 * */

size_t RdPreferences::putChar(const char *key, int8_t value)
{
    if (!_started || !key || _readOnly)
    {
        return 0;
    }
    esp_err_t err = nvs_set_i8(_handle, key, value);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_set_i8 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 1;
}

size_t RdPreferences::putUChar(const char *key, uint8_t value)
{
    if (!_started || !key || _readOnly)
    {
        return 0;
    }
    esp_err_t err = nvs_set_u8(_handle, key, value);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_set_u8 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 1;
}

size_t RdPreferences::putShort(const char *key, int16_t value)
{
    if (!_started || !key || _readOnly)
    {
        return 0;
    }
    esp_err_t err = nvs_set_i16(_handle, key, value);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_set_i16 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 2;
}

size_t RdPreferences::putUShort(const char *key, uint16_t value)
{
    if (!_started || !key || _readOnly)
    {
        return 0;
    }
    esp_err_t err = nvs_set_u16(_handle, key, value);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_set_u16 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 2;
}

size_t RdPreferences::putInt(const char *key, int32_t value)
{
    if (!_started || !key || _readOnly)
    {
        return 0;
    }
    esp_err_t err = nvs_set_i32(_handle, key, value);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_set_i32 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 4;
}

size_t RdPreferences::putUInt(const char *key, uint32_t value)
{
    if (!_started || !key || _readOnly)
    {
        return 0;
    }
    esp_err_t err = nvs_set_u32(_handle, key, value);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_set_u32 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 4;
}

size_t RdPreferences::putLong(const char *key, int32_t value)
{
    return putInt(key, value);
}

size_t RdPreferences::putULong(const char *key, uint32_t value)
{
    return putUInt(key, value);
}

size_t RdPreferences::putLong64(const char *key, int64_t value)
{
    if (!_started || !key || _readOnly)
    {
        return 0;
    }
    esp_err_t err = nvs_set_i64(_handle, key, value);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_set_i64 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 8;
}

size_t RdPreferences::putULong64(const char *key, uint64_t value)
{
    if (!_started || !key || _readOnly)
    {
        return 0;
    }
    esp_err_t err = nvs_set_u64(_handle, key, value);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_set_u64 fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return 8;
}

size_t RdPreferences::putFloat(const char *key, const float value)
{
    return putBytes(key, (void *)&value, sizeof(float));
}

size_t RdPreferences::putDouble(const char *key, const double value)
{
    return putBytes(key, (void *)&value, sizeof(double));
}

size_t RdPreferences::putBool(const char *key, const bool value)
{
    return putUChar(key, (uint8_t)(value ? 1 : 0));
}

size_t RdPreferences::putString(const char *key, const char *value)
{
    if (!_started || !key || !value || _readOnly)
    {
        return 0;
    }
    esp_err_t err = nvs_set_str(_handle, key, value);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_set_str fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return strlen(value);
}

size_t RdPreferences::putString(const char *key, const String value)
{
    return putString(key, value.c_str());
}

size_t RdPreferences::putBytes(const char *key, const void *value, size_t len)
{
    if (!_started || !key || !value || !len || _readOnly)
    {
        return 0;
    }
    esp_err_t err = nvs_set_blob(_handle, key, value, len);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_set_blob fail: %s %s", key, nvs_error(err));
        return 0;
    }
    err = nvs_commit(_handle);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_commit fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return len;
}

/*
 * Get a key value
 * */

int8_t RdPreferences::getChar(const char *key, const int8_t defaultValue)
{
    int8_t value = defaultValue;
    if (!_started || !key)
    {
        return value;
    }
    esp_err_t err = nvs_get_i8(_handle, key, &value);
    if (err)
    {
        LOG_V(MODULE_PREFIX, "nvs_get_i8 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

uint8_t RdPreferences::getUChar(const char *key, const uint8_t defaultValue)
{
    uint8_t value = defaultValue;
    if (!_started || !key)
    {
        return value;
    }
    esp_err_t err = nvs_get_u8(_handle, key, &value);
    if (err)
    {
        LOG_V(MODULE_PREFIX, "nvs_get_u8 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

int16_t RdPreferences::getShort(const char *key, const int16_t defaultValue)
{
    int16_t value = defaultValue;
    if (!_started || !key)
    {
        return value;
    }
    esp_err_t err = nvs_get_i16(_handle, key, &value);
    if (err)
    {
        LOG_V(MODULE_PREFIX, "nvs_get_i16 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

uint16_t RdPreferences::getUShort(const char *key, const uint16_t defaultValue)
{
    uint16_t value = defaultValue;
    if (!_started || !key)
    {
        return value;
    }
    esp_err_t err = nvs_get_u16(_handle, key, &value);
    if (err)
    {
        LOG_V(MODULE_PREFIX, "nvs_get_u16 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

int32_t RdPreferences::getInt(const char *key, const int32_t defaultValue)
{
    int32_t value = defaultValue;
    if (!_started || !key)
    {
        return value;
    }
    esp_err_t err = nvs_get_i32(_handle, key, &value);
    if (err)
    {
        LOG_V(MODULE_PREFIX, "nvs_get_i32 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

uint32_t RdPreferences::getUInt(const char *key, const uint32_t defaultValue)
{
    uint32_t value = defaultValue;
    if (!_started || !key)
    {
        return value;
    }
    esp_err_t err = nvs_get_u32(_handle, key, &value);
    if (err)
    {
        LOG_V(MODULE_PREFIX, "nvs_get_u32 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

int32_t RdPreferences::getLong(const char *key, const int32_t defaultValue)
{
    return getInt(key, defaultValue);
}

uint32_t RdPreferences::getULong(const char *key, const uint32_t defaultValue)
{
    return getUInt(key, defaultValue);
}

int64_t RdPreferences::getLong64(const char *key, const int64_t defaultValue)
{
    int64_t value = defaultValue;
    if (!_started || !key)
    {
        return value;
    }
    esp_err_t err = nvs_get_i64(_handle, key, &value);
    if (err)
    {
        LOG_V(MODULE_PREFIX, "nvs_get_i64 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

uint64_t RdPreferences::getULong64(const char *key, const uint64_t defaultValue)
{
    uint64_t value = defaultValue;
    if (!_started || !key)
    {
        return value;
    }
    esp_err_t err = nvs_get_u64(_handle, key, &value);
    if (err)
    {
        LOG_V(MODULE_PREFIX, "nvs_get_u64 fail: %s %s", key, nvs_error(err));
    }
    return value;
}

float RdPreferences::getFloat(const char *key, const float defaultValue)
{
    float value = defaultValue;
    getBytes(key, (void *)&value, sizeof(float));
    return value;
}

double RdPreferences::getDouble(const char *key, const double defaultValue)
{
    double value = defaultValue;
    getBytes(key, (void *)&value, sizeof(double));
    return value;
}

bool RdPreferences::getBool(const char *key, const bool defaultValue)
{
    return getUChar(key, defaultValue ? 1 : 0) == 1;
}

size_t RdPreferences::getString(const char *key, char *value, const size_t maxLen)
{
    size_t len = 0;
    if (!_started || !key || !value || !maxLen)
    {
        return 0;
    }
    esp_err_t err = nvs_get_str(_handle, key, NULL, &len);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_get_str len fail: %s %s", key, nvs_error(err));
        return 0;
    }
    if (len > maxLen)
    {
        LOG_E(MODULE_PREFIX, "not enough space in value: %u < %u", maxLen, len);
        return 0;
    }
    err = nvs_get_str(_handle, key, value, &len);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_get_str fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return len;
}

String RdPreferences::getString(const char *key, const String defaultValue)
{
    char *value = NULL;
    size_t len = 0;
    if (!_started || !key)
    {
        return String(defaultValue);
    }
    esp_err_t err = nvs_get_str(_handle, key, value, &len);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_get_str len fail: %s %s", key, nvs_error(err));
        return String(defaultValue);
    }
    char buf[len];
    value = buf;
    err = nvs_get_str(_handle, key, value, &len);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_get_str fail: %s %s", key, nvs_error(err));
        return String(defaultValue);
    }
    return String(buf);
}

size_t RdPreferences::getBytesLength(const char *key)
{
    size_t len = 0;
    if (!_started || !key)
    {
        return 0;
    }
    esp_err_t err = nvs_get_blob(_handle, key, NULL, &len);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_get_blob len fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return len;
}

size_t RdPreferences::getBytes(const char *key, void *buf, size_t maxLen)
{
    size_t len = getBytesLength(key);
    if (!len || !buf || !maxLen)
    {
        return len;
    }
    if (len > maxLen)
    {
        LOG_E(MODULE_PREFIX, "not enough space in buffer: %u < %u", maxLen, len);
        return 0;
    }
    esp_err_t err = nvs_get_blob(_handle, key, buf, &len);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "nvs_get_blob fail: %s %s", key, nvs_error(err));
        return 0;
    }
    return len;
}

size_t RdPreferences::freeEntries()
{
    nvs_stats_t nvs_stats;
    esp_err_t err = nvs_get_stats(NULL, &nvs_stats);
    if (err)
    {
        LOG_E(MODULE_PREFIX, "Failed to get nvs statistics");
        return 0;
    }
    return nvs_stats.free_entries;
}
