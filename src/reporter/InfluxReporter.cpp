/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <Arduino.h>
#include <Client.h>

#include "InfluxReporter.h"

using namespace reporter;

size_t InfluxReporter::send(uint32_t timestamp) {

    std::string message = mMeasurement;

    if (mMeasurement.empty() || (mHeaders.empty() && mTags.empty() && mFields.empty())) {
        return 0;
    }

    if (!mClient_p->connect(mHost.c_str(), mPort)) {
        return 0;
    }

    // Add header and tags
    for (const auto &name : mHeaders) {
        message += ',' + std::get<NAME>(name) + '=' + std::get<VALUE>(name);
    }

    for (const auto &name : mTags) {
        message += ',' + std::get<NAME>(name) + '=' + std::get<VALUE>(name);
    }

    // Add measurement fields
    for (auto it = mFields.begin(); it != mFields.end(); ++it) {
        message += (it == mFields.begin()) ? ' ' : ',';
        message += std::get<NAME>(*it) + '=' + std::get<VALUE>(*it);
    }

    // Add timestamp if provided
    if (timestamp != 0 && timestamp != static_cast<uint32_t>(-1)) {
        message += ' ';
        message += std::to_string(timestamp) + "000000000"; // Convert to nanoseconds
    }

    message += '\n';

    const size_t size = mClient_p->write(
        reinterpret_cast<const uint8_t*>(message.c_str()),
        message.length()
    );

    mClient_p->flush();
    mClient_p->stop();

    return size;
}
