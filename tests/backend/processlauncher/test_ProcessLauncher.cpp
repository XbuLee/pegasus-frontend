// Pegasus Frontend
// Copyright (C) 2017  Mátyás Mustoha
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.


#include <QtTest/QtTest>
#include <QProcessEnvironment>

#include "Log.h"
#include "Paths.h"
#include "ProcessLauncher.h"
#include "model/gaming/Game.h"
#include "model/gaming/GameFile.h"


namespace {
QString fallback_workdir() {
#ifdef Q_OS_WIN
    return QStringLiteral("c:\\fallback\\path");
#else
    return QStringLiteral("/fallback/path");
#endif
}
} // namespace


class test_ProcessLauncher : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void exe_path();
    void exe_path_data();

    void workdir_path();
    void workdir_path_data();

    void process_lifecycle();
    void process_launch_failure();
    void appdir_fallback();
};

void test_ProcessLauncher::initTestCase()
{
    Log::init_qttest();
}

void test_ProcessLauncher::cleanupTestCase()
{
    Log::close();
}

void test_ProcessLauncher::exe_path()
{
    QFETCH(QString, cmd);
    QFETCH(QString, basedir);
    QFETCH(QString, expected);

    QCOMPARE(helpers::abs_launchcmd(cmd, basedir), expected);
}

void test_ProcessLauncher::exe_path_data()
{
    const QString app_path = QDir::currentPath();

    QTest::addColumn<QString>("cmd");
    QTest::addColumn<QString>("basedir");
    QTest::addColumn<QString>("expected");

    QTest::newRow("global") << "myapp" << "dummy" << "myapp";
#ifdef Q_OS_WIN
    QTest::newRow("relative A") << ".\\subdir\\app" << "c:\\some\\path" << "C:/some/path/subdir/app";
    QTest::newRow("relative B") << "./subdir/app" << "c:\\some\\path" << "C:/some/path/subdir/app";
    QTest::newRow("relative, no basedir A") << ".\\subdir\\app" << QString() << (app_path + "/subdir/app");
    QTest::newRow("relative, no basedir B") << "./subdir/app" << QString() << (app_path + "/subdir/app");
    QTest::newRow("absolute A ") << "c:\\subdir\\app" << "dummy" << "C:/subdir/app";
    QTest::newRow("absolute B") << "c:/subdir/app" << "dummy" << "C:/subdir/app";
#else
    QTest::newRow("relative") << "./subdir/app" << "/some/path" << "/some/path/subdir/app";
    QTest::newRow("relative, no basedir") << "./subdir/app" << QString() << (app_path + "/subdir/app");
    QTest::newRow("absolute") << "/subdir/app" << "dummy" << "/subdir/app";
#endif
}

void test_ProcessLauncher::workdir_path()
{
    QFETCH(QString, workdir);
    QFETCH(QString, basedir);
    QFETCH(QString, expected);

    QCOMPARE(helpers::abs_workdir(workdir, basedir, fallback_workdir()), expected);
}

void test_ProcessLauncher::workdir_path_data()
{
    const QString app_path = QDir::currentPath();

    QTest::addColumn<QString>("workdir");
    QTest::addColumn<QString>("basedir");
    QTest::addColumn<QString>("expected");

    QTest::newRow("null") << QString() << "dummy" << fallback_workdir();
#ifdef Q_OS_WIN
    QTest::newRow("relative A") << ".\\subdir" << "c:\\some\\path" << "C:/some/path/subdir";
    QTest::newRow("relative B") << "./subdir" << "c:\\some\\path" << "C:/some/path/subdir";
    QTest::newRow("relative, no basedir A") << ".\\subdir" << QString() << (app_path + "/subdir");
    QTest::newRow("relative, no basedir B") << "./subdir" << QString() << (app_path + "/subdir");
    QTest::newRow("absolute A") << "c:\\subdir" << "dummy" << "C:/subdir";
    QTest::newRow("absolute B") << "c:/subdir" << "dummy" << "C:/subdir";
#else
    QTest::newRow("relative") << "./subdir" << "/some/path" << "/some/path/subdir";
    QTest::newRow("relative, no basedir") << "./subdir" << QString() << (app_path + "/subdir");
    QTest::newRow("absolute") << "/subdir" << "dummy" << "/subdir";
#endif
}

void test_ProcessLauncher::process_lifecycle()
{
    model::Game game("test");
#ifdef Q_OS_WIN
    game.setLaunchCmd(QStringLiteral("cmd /q /c exit 0"));
#else
    game.setLaunchCmd(QStringLiteral("/bin/sh -c \"exit 0\""));
#endif
    const QDir temp_dir(QDir::tempPath());
    model::GameFile gamefile(temp_dir.filePath(QStringLiteral("pegasus-test.rom")), game);

    ProcessLauncher launcher;
    QSignalSpy started(&launcher, &ProcessLauncher::processLaunchOk);
    QSignalSpy failed(&launcher, &ProcessLauncher::processLaunchError);
    QSignalSpy finished(&launcher, &ProcessLauncher::processFinished);

    launcher.onLaunchRequested(&gamefile);

    QTRY_COMPARE(started.count(), 1);
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(failed.count(), 0);
}

void test_ProcessLauncher::process_launch_failure()
{
    const QDir temp_dir(QDir::tempPath());
    const QString missing_executable = temp_dir.filePath(
        QStringLiteral("pegasus-executable-that-does-not-exist"));
    model::Game game("test");
    game.setLaunchCmd(missing_executable);
    model::GameFile gamefile(temp_dir.filePath(QStringLiteral("pegasus-test.rom")), game);

    ProcessLauncher launcher;
    QSignalSpy started(&launcher, &ProcessLauncher::processLaunchOk);
    QSignalSpy failed(&launcher, &ProcessLauncher::processLaunchError);
    QSignalSpy finished(&launcher, &ProcessLauncher::processFinished);

    launcher.onLaunchRequested(&gamefile);

    QTRY_COMPARE(failed.count(), 1);
    QTest::qWait(50);
    QCOMPARE(started.count(), 0);
    QCOMPARE(finished.count(), 0);
}

void test_ProcessLauncher::appdir_fallback()
{
#ifdef Q_OS_WIN
    const bool appdir_was_set = qEnvironmentVariableIsSet("APPDIR");
    const QByteArray original_appdir = qgetenv("APPDIR");

    qputenv("APPDIR", QByteArrayLiteral("C:\\external\\pegasus"));
    QCOMPARE(paths::ensure_appdir_env(), QString());
    QCOMPARE(qgetenv("APPDIR"), QByteArrayLiteral("C:\\external\\pegasus"));
    QCOMPARE(
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("appdir")),
        QStringLiteral("C:\\external\\pegasus"));

    qunsetenv("APPDIR");
    const QString fallback = paths::ensure_appdir_env();
    QVERIFY(!fallback.isEmpty());
    QCOMPARE(
        QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv("APPDIR"))),
        QDir::fromNativeSeparators(fallback));
    QCOMPARE(
        QDir::fromNativeSeparators(
            QProcessEnvironment::systemEnvironment().value(QStringLiteral("appdir"))),
        QDir::fromNativeSeparators(fallback));

    if (appdir_was_set)
        qputenv("APPDIR", original_appdir);
    else
        qunsetenv("APPDIR");
#else
    QSKIP("APPDIR fallback is Windows-specific");
#endif
}


QTEST_MAIN(test_ProcessLauncher)
#include "test_ProcessLauncher.moc"
