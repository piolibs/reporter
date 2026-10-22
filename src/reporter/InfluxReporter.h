/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <string>
#include <Client.h>

#include "Reporter.h"

namespace reporter {

/*
 * Sends metrics in the InfluxDB line protocol over a TCP connection,
 * e.g. to a Telegraf socket_listener:
 *
 *   measurement,header=value,tag=value field=value[,field=value] [timestamp]\n
 *
 * Without a batch every send() uses its own connection. Between beginBatch()
 * and commit() the lines are collected and sent in one connection; a batch
 * that would grow over MAX_BATCH is sent early.
 */
class InfluxReporter : public Reporter {
public:
    using Reporter::Reporter;

    static constexpr size_t MAX_LINE = 256;
    static constexpr size_t MAX_BATCH = 1460;  // one TCP segment

    // Returns the line length, 0 if nothing is sent or added to the batch
    size_t send(uint32_t timestamp = 0) override;

    void beginBatch() override;
    size_t commit() override;

private:
    // The current metric as a line, empty if it's invalid or too long
    std::string format(uint32_t timestamp);

    // Write the data in one connection, returns the number of bytes written
    size_t transmit(const std::string &data);

private:
    bool mBatching = false;
    std::string mBatch;
};

} // namespace
