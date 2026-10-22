/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <memory>
#include <string>

#include <Arduino.h>
#include <unity.h>

#include <console.h>
#include <reporter.h>

using namespace reporter;

// Client that records the connections and the data written. The reporter
// keeps a copy of the client, so the state is shared between the copies.
class FakeClient : public Client
{
public:
    struct State
    {
        bool connectFails = false;
        unsigned connects = 0;
        std::string written;    // all data
        std::string lastWrite;  // data of the last connection
    };

    FakeClient() : mState(std::make_shared<State>()) {}

    int connect(IPAddress, uint16_t) override { return connect(""); }
    int connect(const char *host, uint16_t) override { return connect(host); }

    size_t write(uint8_t value) override { return write(&value, 1); }
    size_t write(const uint8_t *buf, size_t size) override
    {
        mState->written.append(reinterpret_cast<const char *>(buf), size);
        mState->lastWrite.append(reinterpret_cast<const char *>(buf), size);
        return size;
    }

    int available() override { return 0; }
    int read() override { return -1; }
    int read(uint8_t *, size_t) override { return 0; }
    int peek() override { return -1; }
    void flush() override {}
    void stop() override {}
    uint8_t connected() override { return 0; }
    operator bool() override { return true; }

    State &state() { return *mState; }

private:
    int connect(const char *)
    {
        if (mState->connectFails)
            return 0;

        ++mState->connects;
        mState->lastWrite.clear();
        return 1;
    }

    std::shared_ptr<State> mState;
};

FakeClient client;
InfluxReporter influx(client, "localhost", 8094);

FakeClient::State &state()
{
    return client.state();
}

void setUp()
{
    // Drop a batch left by a failed test, then start from a clean state
    influx.commit();
    state() = FakeClient::State();

    influx.create("sensor");
    influx.addHeader("device_id", "58BF25DB7E9B");
}

void tearDown() {}

void test_send(void)
{
    influx.addTag("entity_id", "cold");
    influx.addField("value", 384523u);

    const char *expected = "sensor,device_id=58BF25DB7E9B,entity_id=cold value=384523\n";
    TEST_ASSERT_EQUAL(strlen(expected), influx.send());
    TEST_ASSERT_EQUAL(1, state().connects);
    TEST_ASSERT_EQUAL_STRING(expected, state().written.c_str());
}

void test_float_values(void)
{
    // All digits of large values, trailing zeros trimmed
    influx.addField("energy", 16791853.25);
    influx.addField("heat", 38.578559);
    influx.addField("round", 2.0);
    influx.send();

    TEST_ASSERT_EQUAL_STRING("sensor,device_id=58BF25DB7E9B energy=16791853.25,heat=38.578559,round=2.0\n",
                             state().written.c_str());
}

void test_headers_cleared_on_create(void)
{
    // A new measurement starts without the headers of the previous one
    for (int ix = 0; ix < 3; ++ix)
    {
        influx.create("sensor");
        influx.addHeader("device_id", "58BF25DB7E9B");
    }

    influx.addField("value", 1);
    influx.send();

    TEST_ASSERT_EQUAL_STRING("sensor,device_id=58BF25DB7E9B value=1\n", state().written.c_str());
}

void test_clear_keeps_headers(void)
{
    influx.addTag("entity_id", "cold");
    influx.addField("value", 1);
    influx.send();

    influx.clear();
    influx.addTag("entity_id", "hot");
    influx.addField("value", 2);
    influx.send();

    TEST_ASSERT_EQUAL_STRING("sensor,device_id=58BF25DB7E9B,entity_id=cold value=1\n"
                             "sensor,device_id=58BF25DB7E9B,entity_id=hot value=2\n",
                             state().written.c_str());
}

void test_escaping(void)
{
    influx.create("my sensor,1");
    influx.addTag("room name", "living room,1=2");
    influx.addField("note", "say \"hi\" \\ bye");
    influx.addField("value", 1);
    influx.send();

    TEST_ASSERT_EQUAL_STRING("my\\ sensor\\,1,room\\ name=living\\ room\\,1\\=2 "
                             "note=\"say \\\"hi\\\" \\\\ bye\",value=1\n",
                             state().written.c_str());
}

void test_percent_in_measurement(void)
{
    // Not taken as a format string
    influx.create("100%s%n");
    influx.addField("value", 1);
    influx.send();

    TEST_ASSERT_EQUAL_STRING("100%s%n value=1\n", state().written.c_str());
}

void test_timestamp(void)
{
    influx.addField("value", 1);
    influx.send(1761168000);

    TEST_ASSERT_EQUAL_STRING("sensor,device_id=58BF25DB7E9B value=1 1761168000000000000\n",
                             state().written.c_str());
}

void test_no_fields(void)
{
    // The line protocol requires a field
    influx.addTag("entity_id", "cold");

    TEST_ASSERT_EQUAL(0, influx.send());
    TEST_ASSERT_EQUAL(0, state().connects);
}

void test_too_long(void)
{
    influx.addTag("long", std::string(InfluxReporter::MAX_LINE, 'x'));
    influx.addField("value", 1);

    TEST_ASSERT_EQUAL(0, influx.send());
    TEST_ASSERT_EQUAL(0, state().connects);

    // The next metric is sent normally
    influx.clear();
    influx.addField("value", 2);
    TEST_ASSERT_NOT_EQUAL(0, influx.send());
    TEST_ASSERT_EQUAL_STRING("sensor,device_id=58BF25DB7E9B value=2\n", state().written.c_str());
}

void test_batch(void)
{
    influx.beginBatch();

    for (int ix = 1; ix <= 3; ++ix)
    {
        influx.clear();
        influx.addField("value", ix);
        TEST_ASSERT_NOT_EQUAL(0, influx.send());
    }

    // Nothing is sent before commit
    TEST_ASSERT_EQUAL(0, state().connects);

    const char *expected = "sensor,device_id=58BF25DB7E9B value=1\n"
                           "sensor,device_id=58BF25DB7E9B value=2\n"
                           "sensor,device_id=58BF25DB7E9B value=3\n";

    TEST_ASSERT_EQUAL(strlen(expected), influx.commit());
    TEST_ASSERT_EQUAL(1, state().connects);
    TEST_ASSERT_EQUAL_STRING(expected, state().written.c_str());

    // After commit every send uses its own connection again
    influx.clear();
    influx.addField("value", 4);
    influx.send();
    TEST_ASSERT_EQUAL(2, state().connects);
}

void test_batch_overflow(void)
{
    influx.beginBatch();

    // ~40 bytes per line, sent early when the batch would exceed MAX_BATCH
    const int count = 100;
    for (int ix = 0; ix < count; ++ix)
    {
        influx.clear();
        influx.addField("value", ix);
        influx.send();

        TEST_ASSERT_LESS_OR_EQUAL(InfluxReporter::MAX_BATCH, state().lastWrite.size());
    }
    influx.commit();

    TEST_ASSERT_GREATER_THAN(1, state().connects);

    // All lines are delivered in order
    std::string expected;
    for (int ix = 0; ix < count; ++ix)
    {
        expected += "sensor,device_id=58BF25DB7E9B value=" + std::to_string(ix) + "\n";
    }
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), state().written.c_str());
}

void test_connect_failure(void)
{
    state().connectFails = true;

    influx.beginBatch();
    influx.addField("value", 1);
    influx.send();
    TEST_ASSERT_EQUAL(0, influx.commit());

    // The failed batch is dropped, not sent with the next one
    state().connectFails = false;

    influx.beginBatch();
    influx.clear();
    influx.addField("value", 2);
    influx.send();
    influx.commit();

    TEST_ASSERT_EQUAL_STRING("sensor,device_id=58BF25DB7E9B value=2\n", state().written.c_str());
}

void test_empty_commit(void)
{
    influx.beginBatch();
    TEST_ASSERT_EQUAL(0, influx.commit());
    TEST_ASSERT_EQUAL(0, state().connects);
}

void setup()
{
    Serial.begin(74880);
    delay(1000);

    Serial.println("\n\n");
    Serial.flush();

    UNITY_BEGIN();
    RUN_TEST(test_send);
    RUN_TEST(test_float_values);
    RUN_TEST(test_headers_cleared_on_create);
    RUN_TEST(test_clear_keeps_headers);
    RUN_TEST(test_escaping);
    RUN_TEST(test_percent_in_measurement);
    RUN_TEST(test_timestamp);
    RUN_TEST(test_no_fields);
    RUN_TEST(test_too_long);
    RUN_TEST(test_batch);
    RUN_TEST(test_batch_overflow);
    RUN_TEST(test_connect_failure);
    RUN_TEST(test_empty_commit);
    UNITY_END();
}

void loop()
{
    delay(1000);
}
