/*
 * Fujisan Test Suite - Configuration Profile Tests
 *
 * Verifies ConfigurationProfile JSON serialization round-trip and
 * ProfileStorage file I/O, listing, and deletion.
 */

#include <QCoreApplication>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "configurationprofile.h"

class TestProfiles : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    ConfigurationProfile makeFullProfile()
    {
        ConfigurationProfile p;
        p.name = "TestProfile";
        p.description = "Unit test profile";
        p.machineType = "-xe";
        p.videoSystem = "-ntsc";
        p.basicEnabled = false;
        p.altirraOSEnabled = true;
        p.altirraBASICEnabled = true;
        p.osRomPath = "/roms/os.rom";
        p.basicRomPath = "/roms/basic.rom";

        p.enable800Ram = true;
        p.mosaicSize = 128;
        p.axlonSize = 288;
        p.axlonShadow = true;
        p.enableMapRam = false;

        p.turboMode = true;
        p.emulationSpeedIndex = 3;

        p.audioEnabled = false;
        p.audioFrequency = 48000;
        p.audioBits = 8;
        p.audioVolume = 50;
        p.audioBufferLength = 200;
        p.audioLatency = 10;
        p.consoleSound = false;
        p.serialSound = true;
        p.stereoPokey = true;

        p.artifactingMode = "ntsc-old";
        p.showFPS = true;
        p.scalingFilter = false;
        p.integerScaling = false;
        p.keepAspectRatio = false;
        p.overscanFactor = 0.95;
        p.fullscreenMode = true;

        p.palSaturation = 50;
        p.palContrast = -20;
        p.palBrightness = 10;
        p.palGamma = 200;
        p.palTint = -5;
        p.ntscSaturation = 30;
        p.ntscContrast = 40;
        p.ntscBrightness = -10;
        p.ntscGamma = 150;
        p.ntscTint = 15;

        p.joystickEnabled = false;
        p.swapJoysticks = true;
        p.joystick1Device = QStringLiteral("keyboard");
        p.joystick2Device = QStringLiteral("none");
        p.kbdJoy0Enabled = true;
        p.kbdJoy1Enabled = true;
        p.joystick1Preset = "arrows";
        p.joystick2Preset = "numpad";

        p.primaryCartridge.enabled = true;
        p.primaryCartridge.path = "/carts/game.car";
        p.primaryCartridge.type = 1;

        p.disks[0].enabled = true;
        p.disks[0].path = "/disks/d1.atr";
        p.disks[0].readOnly = true;

        p.cassette.enabled = true;
        p.cassette.path = "/tapes/tape.cas";
        p.cassette.bootTape = true;

        p.hardDrives[0].enabled = true;
        p.hardDrives[0].path = "/hd/h1";

        p.netSIOEnabled = true;
        p.rtimeEnabled = true;

        p.printer.enabled = true;
        p.printer.outputFormat = "PDF";
        p.printer.printerType = "Epson FX-80";

        p.xep80Enabled = true;
        p.sioAcceleration = false;

        p.fastbasicBuildPanelEnabled = true;

        return p;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        // Redirect AppDataLocation so ProfileStorage writes into temp dir
        QCoreApplication::setOrganizationName("8bitrelics");
        QCoreApplication::setApplicationName("FujisanTest_Profiles");
        qputenv("XDG_DATA_HOME", m_tempDir.path().toUtf8());
#ifdef Q_OS_MAC
        qputenv("HOME", m_tempDir.path().toUtf8());
#endif
    }

    // ---------------------------------------------------------------
    // JSON round-trip: toJson -> fromJson preserves all fields
    // ---------------------------------------------------------------
    void testJsonRoundTrip()
    {
        ConfigurationProfile original = makeFullProfile();
        QJsonObject json = original.toJson();
        ConfigurationProfile loaded;
        loaded.fromJson(json);

        QCOMPARE(loaded.name, original.name);
        QCOMPARE(loaded.description, original.description);
        QCOMPARE(loaded.machineType, original.machineType);
        QCOMPARE(loaded.videoSystem, original.videoSystem);
        QCOMPARE(loaded.basicEnabled, original.basicEnabled);
        QCOMPARE(loaded.altirraOSEnabled, original.altirraOSEnabled);
        QCOMPARE(loaded.altirraBASICEnabled, original.altirraBASICEnabled);
        QCOMPARE(loaded.osRomPath, original.osRomPath);
        QCOMPARE(loaded.basicRomPath, original.basicRomPath);

        QCOMPARE(loaded.enable800Ram, original.enable800Ram);
        QCOMPARE(loaded.mosaicSize, original.mosaicSize);
        QCOMPARE(loaded.axlonSize, original.axlonSize);
        QCOMPARE(loaded.axlonShadow, original.axlonShadow);
        QCOMPARE(loaded.enableMapRam, original.enableMapRam);

        QCOMPARE(loaded.turboMode, original.turboMode);
        QCOMPARE(loaded.emulationSpeedIndex, original.emulationSpeedIndex);

        QCOMPARE(loaded.audioEnabled, original.audioEnabled);
        QCOMPARE(loaded.audioFrequency, original.audioFrequency);
        QCOMPARE(loaded.audioBits, original.audioBits);
        QCOMPARE(loaded.audioVolume, original.audioVolume);
        QCOMPARE(loaded.audioBufferLength, original.audioBufferLength);
        QCOMPARE(loaded.audioLatency, original.audioLatency);
        QCOMPARE(loaded.consoleSound, original.consoleSound);
        QCOMPARE(loaded.serialSound, original.serialSound);
        QCOMPARE(loaded.stereoPokey, original.stereoPokey);

        QCOMPARE(loaded.artifactingMode, original.artifactingMode);
        QCOMPARE(loaded.showFPS, original.showFPS);
        QCOMPARE(loaded.scalingFilter, original.scalingFilter);
        QCOMPARE(loaded.integerScaling, original.integerScaling);
        QCOMPARE(loaded.keepAspectRatio, original.keepAspectRatio);
        QCOMPARE(loaded.overscanFactor, original.overscanFactor);
        QCOMPARE(loaded.fullscreenMode, original.fullscreenMode);

        QCOMPARE(loaded.palSaturation, original.palSaturation);
        QCOMPARE(loaded.palContrast, original.palContrast);
        QCOMPARE(loaded.palBrightness, original.palBrightness);
        QCOMPARE(loaded.palGamma, original.palGamma);
        QCOMPARE(loaded.palTint, original.palTint);
        QCOMPARE(loaded.ntscSaturation, original.ntscSaturation);
        QCOMPARE(loaded.ntscContrast, original.ntscContrast);
        QCOMPARE(loaded.ntscBrightness, original.ntscBrightness);
        QCOMPARE(loaded.ntscGamma, original.ntscGamma);
        QCOMPARE(loaded.ntscTint, original.ntscTint);

        QCOMPARE(loaded.joystickEnabled, original.joystickEnabled);
        QCOMPARE(loaded.swapJoysticks, original.swapJoysticks);
        QCOMPARE(loaded.joystick1Device, original.joystick1Device);
        QCOMPARE(loaded.joystick2Device, original.joystick2Device);
        QCOMPARE(loaded.kbdJoy0Enabled, original.kbdJoy0Enabled);
        QCOMPARE(loaded.kbdJoy1Enabled, original.kbdJoy1Enabled);
        QCOMPARE(loaded.joystick1Preset, original.joystick1Preset);
        QCOMPARE(loaded.joystick2Preset, original.joystick2Preset);

        QCOMPARE(loaded.primaryCartridge.enabled, original.primaryCartridge.enabled);
        QCOMPARE(loaded.primaryCartridge.path, original.primaryCartridge.path);
        QCOMPARE(loaded.primaryCartridge.type, original.primaryCartridge.type);

        QCOMPARE(loaded.disks[0].enabled, original.disks[0].enabled);
        QCOMPARE(loaded.disks[0].path, original.disks[0].path);
        QCOMPARE(loaded.disks[0].readOnly, original.disks[0].readOnly);

        QCOMPARE(loaded.cassette.enabled, original.cassette.enabled);
        QCOMPARE(loaded.cassette.path, original.cassette.path);
        QCOMPARE(loaded.cassette.bootTape, original.cassette.bootTape);

        QCOMPARE(loaded.hardDrives[0].enabled, original.hardDrives[0].enabled);
        QCOMPARE(loaded.hardDrives[0].path, original.hardDrives[0].path);

        QCOMPARE(loaded.netSIOEnabled, original.netSIOEnabled);
        QCOMPARE(loaded.rtimeEnabled, original.rtimeEnabled);

        QCOMPARE(loaded.printer.enabled, original.printer.enabled);
        QCOMPARE(loaded.printer.outputFormat, original.printer.outputFormat);
        QCOMPARE(loaded.printer.printerType, original.printer.printerType);

        QCOMPARE(loaded.xep80Enabled, original.xep80Enabled);
        QCOMPARE(loaded.sioAcceleration, original.sioAcceleration);
        QCOMPARE(loaded.fastbasicBuildPanelEnabled,
                 original.fastbasicBuildPanelEnabled);
    }

    // ---------------------------------------------------------------
    // fromJson with missing sections falls back to defaults
    // ---------------------------------------------------------------
    void testFromJsonDefaults()
    {
        QJsonObject empty;
        ConfigurationProfile p;
        p.fromJson(empty);

        QCOMPARE(p.machineType, QString("-xl"));
        QCOMPARE(p.videoSystem, QString("-pal"));
        QCOMPARE(p.basicEnabled, true);
        QCOMPARE(p.audioFrequency, 44100);
        QCOMPARE(p.audioVolume, 80);
        QCOMPARE(p.enableMapRam, true);
    }

    // ---------------------------------------------------------------
    // isValid / getDisplayName
    // ---------------------------------------------------------------
    void testIsValid()
    {
        ConfigurationProfile p;
        QVERIFY(!p.isValid()); // name is empty

        p.name = "ValidProfile";
        QVERIFY(p.isValid());
    }

    void testGetDisplayName()
    {
        ConfigurationProfile p;
        p.name = "MyProfile";
        QCOMPARE(p.getDisplayName(), QString("MyProfile"));

        p.description = "Custom setup";
        QCOMPARE(p.getDisplayName(), QString("MyProfile - Custom setup"));
    }

    // ---------------------------------------------------------------
    // ProfileStorage: name validation
    // ---------------------------------------------------------------
    void testValidProfileNames()
    {
        QVERIFY(ProfileStorage::isValidProfileName("MyProfile"));
        QVERIFY(ProfileStorage::isValidProfileName("profile_123"));
        QVERIFY(ProfileStorage::isValidProfileName("a"));

        QVERIFY(!ProfileStorage::isValidProfileName(""));
        QVERIFY(!ProfileStorage::isValidProfileName("file<name"));
        QVERIFY(!ProfileStorage::isValidProfileName("file>name"));
        QVERIFY(!ProfileStorage::isValidProfileName("file:name"));
        QVERIFY(!ProfileStorage::isValidProfileName("file\"name"));
        QVERIFY(!ProfileStorage::isValidProfileName("file|name"));
        QVERIFY(!ProfileStorage::isValidProfileName("file?name"));
        QVERIFY(!ProfileStorage::isValidProfileName("file*name"));
    }

    void testReservedNames()
    {
        QVERIFY(!ProfileStorage::isValidProfileName("CON"));
        QVERIFY(!ProfileStorage::isValidProfileName("con"));
        QVERIFY(!ProfileStorage::isValidProfileName("PRN"));
        QVERIFY(!ProfileStorage::isValidProfileName("NUL"));
        QVERIFY(!ProfileStorage::isValidProfileName("COM1"));
        QVERIFY(!ProfileStorage::isValidProfileName("LPT1"));
    }

    void testSanitizeProfileName()
    {
        QCOMPARE(ProfileStorage::sanitizeProfileName("valid"), QString("valid"));
        QCOMPARE(ProfileStorage::sanitizeProfileName("file<name"),
                 QString("file_name"));
        QCOMPARE(ProfileStorage::sanitizeProfileName("  spaced  "),
                 QString("spaced"));
        QCOMPARE(ProfileStorage::sanitizeProfileName(""),
                 QString("Profile"));

        // Long name truncation
        QString longName(200, 'x');
        QCOMPARE(ProfileStorage::sanitizeProfileName(longName).length(), 100);
    }

    // ---------------------------------------------------------------
    // ProfileStorage: file save / load round-trip
    // ---------------------------------------------------------------
    void testSaveAndLoadProfile()
    {
        ConfigurationProfile original = makeFullProfile();

        QVERIFY(ProfileStorage::saveProfileToFile("test_save", original));
        QVERIFY(ProfileStorage::profileFileExists("test_save"));

        ConfigurationProfile loaded =
            ProfileStorage::loadProfileFromFile("test_save");
        QCOMPARE(loaded.name, original.name);
        QCOMPARE(loaded.machineType, original.machineType);
        QCOMPARE(loaded.audioFrequency, original.audioFrequency);
        QCOMPARE(loaded.primaryCartridge.path, original.primaryCartridge.path);
    }

    // ---------------------------------------------------------------
    // ProfileStorage: listing and deletion
    // ---------------------------------------------------------------
    void testListAndDeleteProfiles()
    {
        // Clean up any profiles left by prior test cases
        for (const QString& name : ProfileStorage::getAvailableProfiles()) {
            ProfileStorage::deleteProfileFile(name);
        }

        ConfigurationProfile p = makeFullProfile();

        ProfileStorage::saveProfileToFile("alpha", p);
        ProfileStorage::saveProfileToFile("beta", p);
        ProfileStorage::saveProfileToFile("gamma", p);

        QStringList profiles = ProfileStorage::getAvailableProfiles();
        QVERIFY(profiles.contains("alpha"));
        QVERIFY(profiles.contains("beta"));
        QVERIFY(profiles.contains("gamma"));
        QCOMPARE(profiles.size(), 3);

        QVERIFY(ProfileStorage::deleteProfileFile("beta"));
        QVERIFY(!ProfileStorage::profileFileExists("beta"));

        profiles = ProfileStorage::getAvailableProfiles();
        QVERIFY(!profiles.contains("beta"));
        QCOMPARE(profiles.size(), 2);
    }

    // ---------------------------------------------------------------
    // ProfileStorage: saving with invalid name fails gracefully
    // ---------------------------------------------------------------
    void testSaveInvalidName()
    {
        ConfigurationProfile p = makeFullProfile();
        QVERIFY(!ProfileStorage::saveProfileToFile("", p));
        QVERIFY(!ProfileStorage::saveProfileToFile("CON", p));
    }

    // ---------------------------------------------------------------
    // ProfileStorage: saving invalid profile (empty name) fails
    // ---------------------------------------------------------------
    void testSaveInvalidProfile()
    {
        ConfigurationProfile p; // name is empty -> isValid() == false
        QVERIFY(!ProfileStorage::saveProfileToFile("valid_key", p));
    }

    // ---------------------------------------------------------------
    // Edge cases: empty strings, extremes
    // ---------------------------------------------------------------
    void testEdgeCaseValues()
    {
        ConfigurationProfile p;
        p.name = "edge";
        p.osRomPath = "";
        p.basicRomPath = "";
        p.mosaicSize = 300; // max
        p.axlonSize = 1056; // max
        p.audioVolume = 0;
        p.overscanFactor = 0.0;

        QJsonObject json = p.toJson();
        ConfigurationProfile loaded;
        loaded.fromJson(json);

        QCOMPARE(loaded.osRomPath, QString(""));
        QCOMPARE(loaded.mosaicSize, 300);
        QCOMPARE(loaded.axlonSize, 1056);
        QCOMPARE(loaded.audioVolume, 0);
        QCOMPARE(loaded.overscanFactor, 0.0);
    }

    // ---------------------------------------------------------------
    // updateLastUsed changes the timestamp
    // ---------------------------------------------------------------
    void testUpdateLastUsed()
    {
        ConfigurationProfile p;
        p.name = "timestamp_test";
        QDateTime before = p.lastUsed;
        QTest::qWait(50);
        p.updateLastUsed();
        QVERIFY(p.lastUsed > before);
    }
};

QTEST_MAIN(TestProfiles)
#include "test_profiles.moc"
