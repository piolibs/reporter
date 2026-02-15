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

class InfluxReporter : public Reporter {
public:
    using Reporter::Reporter;

    size_t send(uint32_t timestamp = 0) override;
};

} // namespace
