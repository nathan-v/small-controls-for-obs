/*
Small Controls for OBS
Copyright (C) 2026 Nathan V

Modelled on OBS Studio's built-in Controls dock (frontend/widgets/OBSBasicControls.cpp),
Copyright (C) 2023 by Lain Bailey, licensed under the GNU GPL v2 or later.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

#include <obs.h>
#include <obs-frontend-api.h>

#include <QFrame>
#include <QIcon>
#include <QPointer>

class QAction;
class QMenu;
class QPushButton;

/*
 * A copy of the OBS Controls dock with shorter labels and a much smaller
 * minimum width, so it fits in a narrow side column.
 *
 *   [ Stream         ] [Go Live]   (broadcast only for YouTube accounts)
 *   [ Record         ] [ || ]
 *   [ Replay Buffer  ] [ save ]    (only when the replay buffer is enabled)
 *   [ Virtual Camera ] [ gear ]    (only when a virtual camera is available)
 *   [ Studio Mode    ]
 *   [ Settings       ]
 *
 * Button states follow the frontend events; actions go through the public
 * frontend API. Two things OBS does not expose (the virtual camera settings
 * dialog and the YouTube broadcast flow) are forwarded to the built-in
 * Controls dock's own buttons, which work even while that dock is hidden.
 */
class SmallControls : public QFrame {
	Q_OBJECT

public:
	explicit SmallControls(QWidget *parent = nullptr);
	~SmallControls() override;

protected:
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	QPushButton *MainButton(const QString &text, const char *objectName);
	QPushButton *IconButton(const char *iconClass, const QString &tooltip, const char *objectName);

	/* state updates */
	void SyncAll();
	void SyncStreaming();
	void SyncRecording();
	void SyncReplayBuffer();
	void SyncVirtualCam();
	void SyncStudioMode();
	void SyncBroadcast();
	void UpdateReplayAvailability();
	void UpdateVirtualCamAvailability();
	bool StreamDelayEnabled();
	void ApplyUniformHeight();
	static void SetLabel(QPushButton *button, const QString &text);
	static void ElideLabel(QPushButton *button);
	static QIcon CameraIcon();

	/* actions */
	void OnStreamClicked();
	void OnForceStopStream();
	void OnRecordClicked();
	void OnPauseClicked();
	void OnReplayClicked();
	void OnSaveReplayClicked();
	void OnVirtualCamClicked();
	void OnVirtualCamConfigClicked();
	void OnBroadcastClicked();
	void OnStudioModeClicked();
	void OnSettingsClicked();

	/* the built-in Controls dock's widgets, looked up by object name */
	QPushButton *BuiltinButton(const char *objectName);

	static void OBSFrontendEvent(enum obs_frontend_event event, void *ptr);

	QPushButton *streamButton = nullptr;
	QPushButton *broadcastButton = nullptr;
	QPushButton *recordButton = nullptr;
	QPushButton *pauseButton = nullptr;
	QPushButton *replayButton = nullptr;
	QPushButton *saveReplayButton = nullptr;
	QPushButton *virtualCamButton = nullptr;
	QPushButton *virtualCamConfigButton = nullptr;
	QPushButton *modeButton = nullptr;
	QPushButton *settingsButton = nullptr;

	QMenu *streamMenu = nullptr;
	QAction *stopStreamAction = nullptr;
	QAction *forceStopStreamAction = nullptr;

	bool ready = false;
	bool shuttingDown = false;
};
