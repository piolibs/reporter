/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#undef LOG_MODULE
#define LOG_MODULE "REPORT"

#include <cstring>

#include <Arduino.h>
#include <Client.h>
#include <console.h>

#include "InfluxReporter.h"

using namespace reporter;

namespace {

// Escape the characters the line protocol uses as separators
void append(std::string &line, const std::string &value, const char *special)
{
    for (char c : value)
    {
        if (strchr(special, c) != nullptr)
        {
            line += '\\';
        }
        line += c;
    }
}

constexpr const char *MEASUREMENT_SPECIAL = ", ";
constexpr const char *KEY_SPECIAL = ",= ";

} // namespace

size_t InfluxReporter::send(uint32_t timestamp)
{
    std::string line = format(timestamp);
    if (line.empty())
    {
        return 0;
    }

    LOGI("%.*s", static_cast<int>(line.size() - 1), line.c_str());  // without '\n'

    if (!mBatching)
    {
        return (transmit(line) == line.size()) ? line.size() : 0;
    }

    if (mBatch.size() + line.size() > MAX_BATCH)
    {
        commit();
        mBatching = true;
    }

    mBatch += line;
    return line.size();
}

void InfluxReporter::beginBatch()
{
    mBatching = true;
    mBatch.clear();
}

size_t InfluxReporter::commit()
{
    size_t length = mBatch.empty() ? 0 : transmit(mBatch);

    mBatching = false;
    mBatch.clear();

    return length;
}

std::string InfluxReporter::format(uint32_t timestamp)
{
    // The line protocol needs at least one field
    if (mMeasurement.empty() || mFields.empty())
    {
        return std::string();
    }

    std::string line;
    line.reserve(MAX_LINE);

    append(line, mMeasurement, MEASUREMENT_SPECIAL);

    for (const auto *tags : {&mHeaders, &mTags})
    {
        for (const auto &tag : *tags)
        {
            line += ',';
            append(line, std::get<NAME>(tag), KEY_SPECIAL);
            line += '=';
            append(line, std::get<VALUE>(tag), KEY_SPECIAL);
        }
    }

    // Field values are already formatted: numbers or quoted strings
    char separator = ' ';
    for (const auto &field : mFields)
    {
        line += separator;
        append(line, std::get<NAME>(field), KEY_SPECIAL);
        line += '=';
        line += std::get<VALUE>(field);
        separator = ',';
    }

    if ((timestamp != 0) && (timestamp != static_cast<uint32_t>(-1)))
    {
        line += ' ';
        line += std::to_string(timestamp);
        line += "000000000";
    }

    line += '\n';

    if (line.size() > MAX_LINE)
    {
        LOGE("failed to send metric, too long: size=%u", line.size());
        return std::string();
    }

    return line;
}

size_t InfluxReporter::transmit(const std::string &data)
{
    if (!mClient_p->connect(mHost.c_str(), mPort))
    {
        LOGE("failed to connect, %u bytes dropped", data.size());
        return 0;
    }

    size_t length = mClient_p->write(reinterpret_cast<const uint8_t *>(data.data()), data.size());

    mClient_p->flush();
    mClient_p->stop();

    if (length != data.size())
    {
        LOGE("failed to send, written %u of %u bytes", length, data.size());
    }

    return length;
}
