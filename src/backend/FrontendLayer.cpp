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


#include "FrontendLayer.h"

#include "Log.h"
#include "Paths.h"
#include "imggen/BlurhashProvider.h"
#include "utils/DiskCachedNAM.h"

#ifdef Q_OS_ANDROID
#include "platform/AndroidAppIconProvider.h"
#endif

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickWindow>
#include <QGuiApplication>
#include <QTimer>
#include <QWindow>

#include <array>

#ifdef Q_OS_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif


namespace {

class DiskCachedNAMFactory : public QQmlNetworkAccessManagerFactory {
public:
    QNetworkAccessManager* create(QObject* parent) override;
};

QNetworkAccessManager* DiskCachedNAMFactory::create(QObject* parent)
{
    return utils::create_disc_cached_nam(parent);
}

void activate_window(QWindow* const window)
{
    window->raise();
    window->requestActivate();

#ifdef Q_OS_WINDOWS
    const HWND handle = reinterpret_cast<HWND>(window->winId());
    if (!handle)
        return;

    if (IsIconic(handle))
        ShowWindow(handle, SW_RESTORE);

    const HWND foreground_window = GetForegroundWindow();
    const DWORD current_thread = GetCurrentThreadId();
    const DWORD foreground_thread = foreground_window
        ? GetWindowThreadProcessId(foreground_window, nullptr)
        : 0;
    const bool input_attached = foreground_thread
        && foreground_thread != current_thread
        && AttachThreadInput(current_thread, foreground_thread, TRUE);

    BringWindowToTop(handle);
    SetForegroundWindow(handle);
    SetActiveWindow(handle);
    SetFocus(handle);

    if (input_attached)
        AttachThreadInput(current_thread, foreground_thread, FALSE);
#endif
}

} // namespace


FrontendLayer::FrontendLayer(QObject* const api_public, QObject* const api_private, QObject* parent)
    : QObject(parent)
    , m_api_public(api_public)
    , m_api_private(api_private)
    , m_engine(nullptr)
    , m_suspended(false)
    , m_windows_hidden(false)
{
    // Note: the pointer to the Api is non-owning and constant during the runtime
}

void FrontendLayer::rebuild()
{
    Q_ASSERT(!m_engine);

    m_engine = new QQmlApplicationEngine(this);
    m_engine->addImportPath(QStringLiteral("lib/qml"));
    m_engine->addImportPath(QStringLiteral("qml"));
    m_engine->setNetworkAccessManagerFactory(new DiskCachedNAMFactory);

    m_engine->addImageProvider(QStringLiteral("blurhash"), new BlurhashProvider);
#ifdef Q_OS_ANDROID
    m_engine->addImageProvider(QStringLiteral("androidicons"), new AndroidAppIconProvider);
#endif

    m_engine->rootContext()->setContextProperty(QStringLiteral("api"), m_api_public);
    m_engine->rootContext()->setContextProperty(QStringLiteral("Api"), m_api_public);
    m_engine->rootContext()->setContextProperty(QStringLiteral("Internal"), m_api_private);
    m_engine->load(QUrl(QStringLiteral("qrc:/frontend/main.qml")));

    emit rebuildComplete();
}

void FrontendLayer::suspendForGame()
{
    Q_ASSERT(m_engine);

    if (m_suspended)
        return;

    m_suspended = true;
    snapshotWindows();
    Log::info(LOGMSG("Frontend prepared for external game"));
}

void FrontendLayer::hideForGame()
{
    if (!m_suspended || m_windows_hidden)
        return;

    for (const WindowState& state : qAsConst(m_window_states)) {
        if (state.window)
            state.window->hide();
    }
    m_windows_hidden = true;
    Log::info(LOGMSG("Frontend windows hidden for external game"));
}

void FrontendLayer::resumeFromGame()
{
    if (!m_suspended)
        return;

    m_suspended = false;
    restoreWindows();
    Log::info(LOGMSG("Frontend resumed after external game"));
}

void FrontendLayer::snapshotWindows()
{
    Q_ASSERT(m_window_states.isEmpty());

    QWindow* const active_window = QGuiApplication::focusWindow();
    for (QWindow* const window : QGuiApplication::topLevelWindows()) {
        if (!window->isVisible())
            continue;

        if (auto* const quick_window = qobject_cast<QQuickWindow*>(window)) {
            quick_window->setPersistentOpenGLContext(true);
            quick_window->setPersistentSceneGraph(true);
        }

        WindowState state;
        state.window = window;
        state.visibility = static_cast<int>(window->visibility());
        state.was_active = window == active_window;
        m_window_states.append(std::move(state));
    }
}

void FrontendLayer::restoreWindows()
{
    QWindow* activation_target = nullptr;
    for (const WindowState& state : qAsConst(m_window_states)) {
        if (!state.window)
            continue;

        if (m_windows_hidden) {
            const auto visibility = static_cast<QWindow::Visibility>(state.visibility);
            state.window->setVisibility(visibility);
        }

        if (!activation_target || state.was_active)
            activation_target = state.window;
    }

    m_window_states.clear();
    m_windows_hidden = false;

    if (activation_target)
        requestWindowActivation(activation_target);
}

void FrontendLayer::requestWindowActivation(QWindow* const window)
{
    static constexpr std::array<int, 3> ACTIVATION_DELAYS_MS {{ 0, 80, 240 }};

    const QPointer<QWindow> guarded_window(window);
    for (const int delay_ms : ACTIVATION_DELAYS_MS) {
        QTimer::singleShot(delay_ms, this, [this, guarded_window](){
            if (m_suspended || !guarded_window)
                return;

            if (QGuiApplication::applicationState() == Qt::ApplicationActive
                && QGuiApplication::focusWindow() == guarded_window.data())
            {
                return;
            }

            activate_window(guarded_window.data());
        });
    }
}

void FrontendLayer::clearCache()
{
    Q_ASSERT(m_engine);
    m_engine->clearComponentCache();
}
