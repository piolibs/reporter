/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Reporter.h"

using namespace reporter;

void Reporter::create(const std::string &measurement) {
    mMeasurement = measurement;
    mHeaders.clear();
    mTags.clear();
    mFields.clear();
}

void Reporter::clear() {
    mTags.clear();
    mFields.clear();
}

void Reporter::destroy() {
    mMeasurement.clear();
    mHeaders.clear();
    mTags.clear();
    mFields.clear();
}

// Tags are stored as is, the reporter escapes them for its format
void Reporter::addHeader(const std::string &tag, const std::string &value, bool condition) {
    if (condition) {
        mHeaders.emplace_back(tag, value);
    }
}

void Reporter::addTag(const std::string &tag, const std::string &value, bool condition) {
    if (condition) {
        mTags.emplace_back(tag, value);
    }
}

// A string field is quoted, quotes and backslashes inside are escaped
void Reporter::addField(const std::string &tag, const std::string &value, bool condition) {
    if (!condition) return;

    std::string quoted = "\"";
    for (char c : value) {
        if ((c == '"') || (c == '\\')) {
            quoted += '\\';
        }
        quoted += c;
    }
    quoted += '"';

    mFields.emplace_back(tag, quoted);
}
