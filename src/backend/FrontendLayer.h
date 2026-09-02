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


#pragma once

#include <QObject>
#include <QPointer>
#include <QVector>

class QQmlApplicationEngine;
class QWindow;


/// Owns the QML frontend and suspends it while external games are running.
class FrontendLayer : public QObject {
    Q_OBJECT

public:
    explicit FrontendLayer(QObject* const api_public, QObject* const api_private, QObject* parent = nullptr);

    void rebuild();
    void suspendForGame();
    void hideForGame();
    void resumeFromGame();

    void clearCache();

signals:
    void rebuildComplete();

private:
    struct WindowState {
        QPointer<QWindow> window;
        int visibility = 0;
        bool was_active = false;
    };

    QObject* const m_api_public;
    QObject* const m_api_private;
    QQmlApplicationEngine* m_engine;
    QVector<WindowState> m_window_states;
    bool m_suspended;
    bool m_windows_hidden;

    void snapshotWindows();
    void restoreWindows();
    void requestWindowActivation(QWindow*);
};
