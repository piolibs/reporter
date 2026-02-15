/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <memory>
#include <list>
#include <string>
#include <tuple>
#include <type_traits>
#include <Arduino.h>
#include <Client.h>

namespace reporter {

class Reporter {
public:

    Reporter(Client& client, const std::string &host, uint16_t port)
      : mClient_p(std::shared_ptr<Client>(&client, [](Client*){})), // no deleter
        mHost(host),
        mPort(port) {}

    template<typename T>
    Reporter(T&& client, const std::string &host, uint16_t port)
      : mClient_p(std::make_shared<std::decay_t<T>>(std::forward<T>(client))), // create copy
        mHost(host),
        mPort(port) {
        static_assert(std::is_base_of_v<Client, std::decay_t<T>>, "T must inherit from Client");
    }

    virtual ~Reporter() = default;

    virtual void create(const std::string &measurement);
    virtual void addHeader(const std::string &tag, const std::string &value, bool condition = true);
    virtual void addTag(const std::string &tag, const std::string &value, bool condition = true);
    virtual void addField(const std::string &tag, const std::string &value, bool condition = true);

    virtual void clear();
    virtual void destroy();

    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
    void addHeader(const std::string &tag, const T &value, bool condition = true) {
        if (condition) {
            mHeaders.emplace_back(tag, std::to_string(value));
        }
    }

    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
    void addTag(const std::string &tag, const T &value, bool condition = true) {
        if (condition) {
            mTags.emplace_back(tag, std::to_string(value));
        }
    }

    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
    void addField(const std::string &tag, const T &value, bool condition = true) {
        if (condition) {
            mFields.emplace_back(tag, std::to_string(value));
        }
    }

    virtual size_t send(uint32_t timestamp = 0) = 0;

protected:
    enum MetricFields { NAME, VALUE };
    using Metric = std::tuple<std::string, std::string>;

protected:
    std::shared_ptr<Client> mClient_p;

    std::string mHost;
    uint16_t mPort;

    std::string mMeasurement;
    std::list<Metric> mHeaders;
    std::list<Metric> mTags;
    std::list<Metric> mFields;
};

} // namespace
