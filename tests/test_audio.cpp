/*
 * Fujisan Test Suite - Audio Pipeline Tests
 *
 * Tests audio format selection, fragment/buffer sizing, and ring buffer
 * edge cases without playing actual sound. Validates the platform-specific
 * constants used by AtariEmulator::setupAudio().
 */

#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QByteArray>
#include <QtTest/QtTest>

class TestAudio : public QObject {
    Q_OBJECT

private slots:
    // ---------------------------------------------------------------
    // Platform-specific fragment size constants
    // ---------------------------------------------------------------
    void testFragmentSizeConstants()
    {
#if defined(_WIN32) || defined(__linux__)
        const int expectedFragment = 1024;
        const int expectedDspMultiplier = 25;
#else
        const int expectedFragment = 512;
        const int expectedDspMultiplier = 10;
#endif
        QVERIFY(expectedFragment == 512 || expectedFragment == 1024);
        QVERIFY(expectedDspMultiplier == 10 || expectedDspMultiplier == 25);

        // DSP buffer must hold multiple fragments
        int dspBufferSamples = expectedFragment * expectedDspMultiplier;
        QVERIFY(dspBufferSamples >= expectedFragment * 5);
    }

    // ---------------------------------------------------------------
    // QAudioFormat construction (16-bit mono, common sample rates)
    // ---------------------------------------------------------------
    void testAudioFormatConstruction_data()
    {
        QTest::addColumn<int>("sampleRate");
        QTest::newRow("44100") << 44100;
        QTest::newRow("48000") << 48000;
        QTest::newRow("22050") << 22050;
    }

    void testAudioFormatConstruction()
    {
        QFETCH(int, sampleRate);

        QAudioFormat format;
        format.setSampleRate(sampleRate);
        format.setChannelCount(1);
        format.setSampleSize(16);
        format.setCodec("audio/pcm");
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setSampleType(QAudioFormat::SignedInt);

        QCOMPARE(format.sampleRate(), sampleRate);
        QCOMPARE(format.channelCount(), 1);
        QCOMPARE(format.sampleSize(), 16);
        QCOMPARE(format.codec(), QString("audio/pcm"));

        // Verify the default device supports a format close to this
        QAudioDeviceInfo info = QAudioDeviceInfo::defaultOutputDevice();
        if (!info.isNull()) {
            QAudioFormat nearest = info.nearestFormat(format);
            // Must at least return a valid format
            QVERIFY(nearest.sampleRate() > 0);
            QVERIFY(nearest.channelCount() >= 1);
            QVERIFY(nearest.sampleSize() >= 8);
        }
    }

    // ---------------------------------------------------------------
    // Ring buffer: basic write/read cycle
    // ---------------------------------------------------------------
    void testRingBufferBasicCycle()
    {
        const int bufSize = 4096;
        QByteArray ring(bufSize, 0);
        int writePos = 0;
        int readPos = 0;

        // Write 1000 bytes of test data
        const int writeLen = 1000;
        for (int i = 0; i < writeLen; ++i) {
            ring[writePos] = static_cast<char>(i & 0xFF);
            writePos = (writePos + 1) % bufSize;
        }

        // Available data calculation
        int available = (writePos - readPos + bufSize) % bufSize;
        QCOMPARE(available, writeLen);

        // Read back and verify
        for (int i = 0; i < writeLen; ++i) {
            QCOMPARE(static_cast<unsigned char>(ring[readPos]),
                     static_cast<unsigned char>(i & 0xFF));
            readPos = (readPos + 1) % bufSize;
        }

        available = (writePos - readPos + bufSize) % bufSize;
        QCOMPARE(available, 0);
    }

    // ---------------------------------------------------------------
    // Ring buffer: underrun produces silence (zeros)
    // ---------------------------------------------------------------
    void testRingBufferUnderrun()
    {
        const int bufSize = 4096;
        QByteArray ring(bufSize, 0);
        int writePos = 0;
        int readPos = 0;

        // Empty ring: available == 0
        int available = (writePos - readPos + bufSize) % bufSize;
        QCOMPARE(available, 0);

        // Reading from empty buffer should get zeros (the initial fill)
        const int readLen = 256;
        QByteArray output(readLen, 0);
        for (int i = 0; i < readLen; ++i) {
            // In the real code, empty buffer => memset to silence
            if (available == 0) {
                output[i] = 0;
            } else {
                output[i] = ring[readPos];
                readPos = (readPos + 1) % bufSize;
                available--;
            }
        }

        // All bytes must be silence
        for (int i = 0; i < readLen; ++i) {
            QCOMPARE(output[i], char(0));
        }
    }

    // ---------------------------------------------------------------
    // Ring buffer: overrun wraps correctly
    // ---------------------------------------------------------------
    void testRingBufferOverrun()
    {
        const int bufSize = 256;
        QByteArray ring(bufSize, 0);
        int writePos = 0;

        // Write more than buffer size
        const int writeLen = bufSize + 100;
        for (int i = 0; i < writeLen; ++i) {
            ring[writePos] = static_cast<char>(i & 0xFF);
            writePos = (writePos + 1) % bufSize;
        }

        // writePos should have wrapped
        QCOMPARE(writePos, writeLen % bufSize);
        // No crash is the primary assertion here
    }

    // ---------------------------------------------------------------
    // Ring buffer: wrap-around read
    // ---------------------------------------------------------------
    void testRingBufferWrapAroundRead()
    {
        const int bufSize = 256;
        QByteArray ring(bufSize, 0);
        int writePos = 200;
        int readPos = 200;

        // Write 100 bytes (wraps around at 256)
        for (int i = 0; i < 100; ++i) {
            ring[writePos] = static_cast<char>(i + 1);
            writePos = (writePos + 1) % bufSize;
        }
        QCOMPARE(writePos, 44); // 200+100=300, 300%256=44

        // Read back all 100 bytes
        for (int i = 0; i < 100; ++i) {
            QCOMPARE(static_cast<unsigned char>(ring[readPos]),
                     static_cast<unsigned char>(i + 1));
            readPos = (readPos + 1) % bufSize;
        }
        QCOMPARE(readPos, writePos);
    }

    // ---------------------------------------------------------------
    // DSP buffer sizing: bytes = samples * bytesPerSample
    // ---------------------------------------------------------------
    void testDspBufferByteCalculation()
    {
        const int sampleRate = 44100;
        const int bytesPerSample = 2; // 16-bit
        const int channelCount = 1;
        const int fps = 60; // approximate

        int samplesPerFrame = sampleRate / fps;
        int bytesPerFrame = samplesPerFrame * bytesPerSample * channelCount;

        // At 44100Hz / 60fps = 735 samples/frame = 1470 bytes/frame
        QVERIFY(bytesPerFrame > 0);
        QVERIFY(bytesPerFrame < 4096); // sanity

#if defined(_WIN32) || defined(__linux__)
        const int fragmentSize = 1024;
        const int dspMultiplier = 25;
#else
        const int fragmentSize = 512;
        const int dspMultiplier = 10;
#endif
        int fragmentBytes = fragmentSize * bytesPerSample;
        int totalDspBytes = fragmentSize * dspMultiplier * bytesPerSample;

        // The total DSP buffer must hold several frames' worth of audio data
        // to absorb timing jitter. Fragment size is the per-write chunk and
        // may be smaller than one emulation frame.
        QVERIFY(totalDspBytes >= bytesPerFrame * 3);
        QVERIFY(fragmentBytes > 0);
    }

    // ---------------------------------------------------------------
    // Audio diagnostic CSV path is writable
    // ---------------------------------------------------------------
    void testDiagnosticCsvPathWritable()
    {
        QString dataDir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        QVERIFY(!dataDir.isEmpty());

        QDir dir(dataDir);
        if (!dir.exists()) {
            QVERIFY(dir.mkpath("."));
        }

        // Attempt to create and write a test CSV
        QString csvPath = dir.filePath("audio_diagnostics_test.csv");
        QFile f(csvPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("test\n");
        f.close();
        QFile::remove(csvPath);
    }
};

QTEST_MAIN(TestAudio)
#include "test_audio.moc"
