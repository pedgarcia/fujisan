/*
 * Fujisan Test Suite - ROM Loading / Argument Construction Tests
 *
 * Tests the argv-building logic used by AtariEmulator::initializeWithDisplayConfig()
 * without depending on libatari800. We replicate the argument construction logic
 * from atariemulator.cpp and verify it produces correct argument lists.
 *
 * Also tests file extension routing for loadFile().
 */

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtTest/QtTest>

// Extracted argument-building logic (mirrors atariemulator.cpp)
static QStringList buildArgList(
    const QString& machineType,
    const QString& videoSystem,
    const QString& artifactMode,
    bool audioEnabled,
    int audioFreq,
    bool altirraOSEnabled,
    const QString& osRomPath,
    bool altirraBASICEnabled,
    const QString& basicRomPath,
    bool basicEnabled)
{
    QStringList argList;
    argList << "atari800";
    argList << machineType;
    argList << videoSystem;

    if (artifactMode != "none") {
        if (videoSystem == "-ntsc") {
            if (artifactMode == "ntsc-old" || artifactMode == "ntsc-new") {
                argList << "-ntsc-artif" << artifactMode;
            }
        } else if (videoSystem == "-pal") {
            if (artifactMode == "ntsc-old" || artifactMode == "ntsc-new") {
                argList << "-pal-artif" << "pal-simple";
            }
        }
    }

    if (audioEnabled) {
        argList << "-sound";
        argList << "-dsprate" << QString::number(audioFreq);
        argList << "-audio16";
        argList << "-volume" << "80";
    } else {
        argList << "-nosound";
    }

    if (altirraOSEnabled) {
        if (machineType == "-5200") {
            argList << "-5200-rev" << "altirra";
        } else if (machineType == "-atari") {
            argList << "-800-rev" << "altirra";
        } else {
            argList << "-xl-rev" << "altirra";
        }
    } else {
        if (!osRomPath.isEmpty()) {
            QFileInfo osRomFile(osRomPath);
            if (osRomFile.exists()) {
                if (machineType == "-5200") {
                    argList << "-5200-rev" << "AUTO";
                    argList << "-5200_rom" << osRomPath;
                } else if (machineType == "-atari") {
                    argList << "-800-rev" << "AUTO";
                    argList << "-osb_rom" << osRomPath;
                } else {
                    argList << "-xl-rev" << "AUTO";
                    argList << "-xlxe_rom" << osRomPath;
                }
            } else {
                if (machineType == "-5200") {
                    argList << "-5200-rev" << "altirra";
                } else if (machineType == "-atari") {
                    argList << "-800-rev" << "altirra";
                } else {
                    argList << "-xl-rev" << "altirra";
                }
            }
        } else {
            if (machineType == "-5200") {
                argList << "-5200-rev" << "altirra";
            } else if (machineType == "-atari") {
                argList << "-800-rev" << "altirra";
            } else {
                argList << "-xl-rev" << "altirra";
            }
        }
    }

    if (altirraBASICEnabled) {
        argList << "-basic-rev" << "altirra";
    } else if (!basicRomPath.isEmpty()) {
        QFileInfo basicRomFile(basicRomPath);
        if (basicRomFile.exists()) {
            argList << "-basic-rev" << "AUTO";
            argList << "-basic_rom" << basicRomPath;
        }
    }

    if (basicEnabled) {
        argList << "-basic";
    } else {
        argList << "-nobasic";
    }

    return argList;
}

// Determine load method based on file extension (mirrors AtariEmulator::loadFile)
enum LoadMethod { BinLoad, Cartridge, Unknown };
static LoadMethod classifyFileExtension(const QString& filename)
{
    QFileInfo fi(filename);
    QString ext = fi.suffix().toLower();
    if (ext == "xex" || ext == "exe" || ext == "com")
        return BinLoad;
    if (ext == "car" || ext == "rom" || ext == "bin" || ext == "atr")
        return Cartridge;
    return Cartridge; // default path
}

class TestRomLoading : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        QCoreApplication::setOrganizationName("8bitrelics");
        QCoreApplication::setApplicationName("Fujisan");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_tempDir.path());
    }

    // ---------------------------------------------------------------
    // Default XL build with Altirra OS + audio
    // ---------------------------------------------------------------
    void testDefaultXLAltirra()
    {
        QStringList args = buildArgList(
            "-xl", "-pal", "none",
            /*audio=*/true, 44100,
            /*altirra=*/true, "",
            /*altirraBASIC=*/false, "",
            /*basic=*/false);

        QVERIFY(args.contains("-xl"));
        QVERIFY(args.contains("-pal"));
        QVERIFY(args.contains("-sound"));
        QVERIFY(args.contains("-xl-rev"));
        QCOMPARE(args[args.indexOf("-xl-rev") + 1], QString("altirra"));
        QVERIFY(args.contains("-nobasic"));
        QVERIFY(!args.contains("-nosound"));
    }

    // ---------------------------------------------------------------
    // XE machine with NTSC
    // ---------------------------------------------------------------
    void testXENtsc()
    {
        QStringList args = buildArgList(
            "-xe", "-ntsc", "none",
            true, 44100, true, "", false, "", false);

        QVERIFY(args.contains("-xe"));
        QVERIFY(args.contains("-ntsc"));
        QVERIFY(args.contains("-xl-rev"));
        QCOMPARE(args[args.indexOf("-xl-rev") + 1], QString("altirra"));
    }

    // ---------------------------------------------------------------
    // 5200 machine
    // ---------------------------------------------------------------
    void test5200Machine()
    {
        QStringList args = buildArgList(
            "-5200", "-ntsc", "none",
            true, 44100, true, "", false, "", false);

        QVERIFY(args.contains("-5200"));
        QVERIFY(args.contains("-5200-rev"));
        QCOMPARE(args[args.indexOf("-5200-rev") + 1], QString("altirra"));
        QVERIFY(!args.contains("-xl-rev"));
    }

    // ---------------------------------------------------------------
    // Atari 800
    // ---------------------------------------------------------------
    void testAtari800Machine()
    {
        QStringList args = buildArgList(
            "-atari", "-pal", "none",
            true, 44100, true, "", false, "", true);

        QVERIFY(args.contains("-atari"));
        QVERIFY(args.contains("-800-rev"));
        QCOMPARE(args[args.indexOf("-800-rev") + 1], QString("altirra"));
        QVERIFY(args.contains("-basic"));
    }

    // ---------------------------------------------------------------
    // Custom OS ROM path (existing file)
    // ---------------------------------------------------------------
    void testCustomOSRom()
    {
        QTemporaryFile romFile(m_tempDir.path() + "/XXXXXX.rom");
        romFile.open();
        romFile.write("fake rom");
        romFile.flush();

        QStringList args = buildArgList(
            "-xl", "-pal", "none",
            true, 44100,
            /*altirra=*/false, romFile.fileName(),
            false, "", false);

        QVERIFY(args.contains("-xl-rev"));
        QCOMPARE(args[args.indexOf("-xl-rev") + 1], QString("AUTO"));
        QVERIFY(args.contains("-xlxe_rom"));
        QCOMPARE(args[args.indexOf("-xlxe_rom") + 1], romFile.fileName());
    }

    // ---------------------------------------------------------------
    // Custom OS ROM for 5200 (existing file)
    // ---------------------------------------------------------------
    void testCustomOSRom5200()
    {
        QTemporaryFile romFile(m_tempDir.path() + "/XXXXXX.rom");
        romFile.open();
        romFile.write("fake rom");
        romFile.flush();

        QStringList args = buildArgList(
            "-5200", "-ntsc", "none",
            true, 44100,
            false, romFile.fileName(),
            false, "", false);

        QVERIFY(args.contains("-5200-rev"));
        QCOMPARE(args[args.indexOf("-5200-rev") + 1], QString("AUTO"));
        QVERIFY(args.contains("-5200_rom"));
    }

    // ---------------------------------------------------------------
    // Custom OS ROM for 800 (existing file)
    // ---------------------------------------------------------------
    void testCustomOSRom800()
    {
        QTemporaryFile romFile(m_tempDir.path() + "/XXXXXX.rom");
        romFile.open();
        romFile.write("fake rom");
        romFile.flush();

        QStringList args = buildArgList(
            "-atari", "-pal", "none",
            true, 44100,
            false, romFile.fileName(),
            false, "", false);

        QVERIFY(args.contains("-800-rev"));
        QCOMPARE(args[args.indexOf("-800-rev") + 1], QString("AUTO"));
        QVERIFY(args.contains("-osb_rom"));
    }

    // ---------------------------------------------------------------
    // Missing OS ROM falls back to Altirra
    // ---------------------------------------------------------------
    void testMissingOSRomFallback()
    {
        QStringList args = buildArgList(
            "-xl", "-pal", "none",
            true, 44100,
            false, "/nonexistent/path.rom",
            false, "", false);

        QVERIFY(args.contains("-xl-rev"));
        QCOMPARE(args[args.indexOf("-xl-rev") + 1], QString("altirra"));
        QVERIFY(!args.contains("-xlxe_rom"));
    }

    // ---------------------------------------------------------------
    // Empty OS ROM path also falls back to Altirra
    // ---------------------------------------------------------------
    void testEmptyOSRomPath()
    {
        QStringList args = buildArgList(
            "-xl", "-pal", "none",
            true, 44100, false, "", false, "", false);

        QVERIFY(args.contains("-xl-rev"));
        QCOMPARE(args[args.indexOf("-xl-rev") + 1], QString("altirra"));
    }

    // ---------------------------------------------------------------
    // Audio disabled
    // ---------------------------------------------------------------
    void testAudioDisabled()
    {
        QStringList args = buildArgList(
            "-xl", "-pal", "none",
            /*audio=*/false, 44100,
            true, "", false, "", false);

        QVERIFY(args.contains("-nosound"));
        QVERIFY(!args.contains("-sound"));
        QVERIFY(!args.contains("-dsprate"));
    }

    // ---------------------------------------------------------------
    // Custom audio frequency
    // ---------------------------------------------------------------
    void testCustomAudioFrequency()
    {
        QStringList args = buildArgList(
            "-xl", "-pal", "none",
            true, 22050, true, "", false, "", false);

        QVERIFY(args.contains("-dsprate"));
        QCOMPARE(args[args.indexOf("-dsprate") + 1], QString("22050"));
    }

    // ---------------------------------------------------------------
    // Altirra BASIC
    // ---------------------------------------------------------------
    void testAltirraBASIC()
    {
        QStringList args = buildArgList(
            "-xl", "-pal", "none",
            true, 44100, true, "",
            /*altirraBASIC=*/true, "", true);

        QVERIFY(args.contains("-basic-rev"));
        QCOMPARE(args[args.indexOf("-basic-rev") + 1], QString("altirra"));
        QVERIFY(args.contains("-basic"));
    }

    // ---------------------------------------------------------------
    // Custom BASIC ROM
    // ---------------------------------------------------------------
    void testCustomBASICRom()
    {
        QTemporaryFile basicFile(m_tempDir.path() + "/XXXXXX.rom");
        basicFile.open();
        basicFile.write("fake basic");
        basicFile.flush();

        QStringList args = buildArgList(
            "-xl", "-pal", "none",
            true, 44100, true, "",
            false, basicFile.fileName(), true);

        QVERIFY(args.contains("-basic-rev"));
        QCOMPARE(args[args.indexOf("-basic-rev") + 1], QString("AUTO"));
        QVERIFY(args.contains("-basic_rom"));
    }

    // ---------------------------------------------------------------
    // NTSC artifact mode
    // ---------------------------------------------------------------
    void testNtscArtifactMode()
    {
        QStringList args = buildArgList(
            "-xl", "-ntsc", "ntsc-old",
            true, 44100, true, "", false, "", false);

        QVERIFY(args.contains("-ntsc-artif"));
        QCOMPARE(args[args.indexOf("-ntsc-artif") + 1], QString("ntsc-old"));
    }

    // ---------------------------------------------------------------
    // PAL artifact mode (NTSC mode maps to pal-simple)
    // ---------------------------------------------------------------
    void testPalArtifactMapping()
    {
        QStringList args = buildArgList(
            "-xl", "-pal", "ntsc-old",
            true, 44100, true, "", false, "", false);

        QVERIFY(args.contains("-pal-artif"));
        QCOMPARE(args[args.indexOf("-pal-artif") + 1], QString("pal-simple"));
        QVERIFY(!args.contains("-ntsc-artif"));
    }

    // ---------------------------------------------------------------
    // Artifact mode "none" adds nothing
    // ---------------------------------------------------------------
    void testNoArtifact()
    {
        QStringList args = buildArgList(
            "-xl", "-ntsc", "none",
            true, 44100, true, "", false, "", false);

        QVERIFY(!args.contains("-ntsc-artif"));
        QVERIFY(!args.contains("-pal-artif"));
    }

    // ---------------------------------------------------------------
    // File extension classification
    // ---------------------------------------------------------------
    void testFileExtensionRouting()
    {
        QCOMPARE(classifyFileExtension("game.xex"), BinLoad);
        QCOMPARE(classifyFileExtension("game.XEX"), BinLoad);
        QCOMPARE(classifyFileExtension("program.exe"), BinLoad);
        QCOMPARE(classifyFileExtension("autorun.com"), BinLoad);
        QCOMPARE(classifyFileExtension("cart.car"), Cartridge);
        QCOMPARE(classifyFileExtension("firmware.rom"), Cartridge);
        QCOMPARE(classifyFileExtension("firmware.bin"), Cartridge);
        QCOMPARE(classifyFileExtension("disk.atr"), Cartridge);
        QCOMPARE(classifyFileExtension("unknown.xyz"), Cartridge);
    }

    // ---------------------------------------------------------------
    // First arg is always "atari800"
    // ---------------------------------------------------------------
    void testFirstArgIsAtari800()
    {
        QStringList args = buildArgList(
            "-xl", "-pal", "none",
            true, 44100, true, "", false, "", false);
        QCOMPARE(args.first(), QString("atari800"));
    }
};

QTEST_MAIN(TestRomLoading)
#include "test_rom_loading.moc"
