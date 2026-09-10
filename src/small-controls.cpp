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

#include "small-controls.hpp"

#include <obs-module.h>
#include <util/config-file.h>

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QShowEvent>
#include <QStyle>
#include <QStyleOptionButton>
#include <QVBoxLayout>

#include <cstring>

/* The built-in dock forces 150 px on its wide buttons; this is the whole point. */
#define MIN_BUTTON_WIDTH 60

/* Output id OBS checks to decide whether the virtual camera is available. */
#define VIRTUAL_CAM_ID "virtualcam_output"

/* ------------------------------------------------------------------------- */
/* helpers                                                                   */

static QString FrontendStr(const char *key)
{
	const char *s = obs_frontend_get_locale_string(key);
	return QString::fromUtf8(s && *s ? s : key);
}

static QString PluginStr(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

/* OBS themes style buttons through the "class" property: "state-active"
 * colours an active Stream/Record button, "icon-*" draws the icon buttons. */
static void SetClasses(QWidget *widget, const QString &classes)
{
	widget->setProperty("class", classes);
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
}

/* ------------------------------------------------------------------------- */
/* construction                                                              */

QPushButton *SmallControls::MainButton(const QString &text, const char *objectName)
{
	QPushButton *b = new QPushButton(this);
	b->setObjectName(objectName);
	/* Ignored: the button never asks the layout for room, so the dock can be
	 * as narrow as MIN_BUTTON_WIDTH plus the icon button beside it. */
	b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	b->setMinimumWidth(MIN_BUTTON_WIDTH);
	b->installEventFilter(this); /* re-elide the label on resize */
	SetLabel(b, text);
	return b;
}

QPushButton *SmallControls::IconButton(const char *iconClass, const QString &tooltip, const char *objectName)
{
	QPushButton *b = new QPushButton(this);
	b->setObjectName(objectName);
	b->setToolTip(tooltip);
	b->setAccessibleName(tooltip);
	b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	SetClasses(b, iconClass);
	return b;
}

SmallControls::SmallControls(QWidget *parent) : QFrame(parent)
{
	QVBoxLayout *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);

	/* Same widget tree as the built-in dock so themes style it the same way. */
	QFrame *frame = new QFrame(this);
	frame->setObjectName("controlsFrame");
	frame->setFrameShape(QFrame::NoFrame);
	outer->addWidget(frame);

	QVBoxLayout *layout = new QVBoxLayout(frame);

	/* stream + broadcast */
	streamButton = MainButton(PluginStr("SmallControls.Stream"), "scStreamButton");
	broadcastButton = MainButton(FrontendStr("Basic.Main.SetupBroadcast"), "scBroadcastButton");
	QHBoxLayout *row = new QHBoxLayout();
	row->addWidget(streamButton);
	row->addWidget(broadcastButton);
	layout->addLayout(row);

	/* record + pause */
	recordButton = MainButton(PluginStr("SmallControls.Record"), "scRecordButton");
	pauseButton = IconButton("icon-media-pause", FrontendStr("Basic.Main.PauseRecording"), "scPauseButton");
	row = new QHBoxLayout();
	row->addWidget(recordButton);
	row->addWidget(pauseButton);
	layout->addLayout(row);

	/* replay buffer + save */
	replayButton = MainButton(PluginStr("SmallControls.ReplayBuffer"), "scReplayButton");
	saveReplayButton = IconButton("icon-save", FrontendStr("Basic.Main.SaveReplay"), "scSaveReplayButton");
	row = new QHBoxLayout();
	row->addWidget(replayButton);
	row->addWidget(saveReplayButton);
	layout->addLayout(row);

	/* virtual camera + config */
	virtualCamButton = MainButton(PluginStr("SmallControls.VirtualCam"), "scVirtualCamButton");
	virtualCamButton->setProperty("narrowIcon", true); /* camera icon when the label won't fit */
	virtualCamConfigButton =
		IconButton("icon-gear", FrontendStr("Basic.Main.VirtualCamConfig"), "scVirtualCamConfigButton");
	row = new QHBoxLayout();
	row->addWidget(virtualCamButton);
	row->addWidget(virtualCamConfigButton);
	layout->addLayout(row);

	/* studio mode, settings */
	modeButton = MainButton(FrontendStr("Basic.TogglePreviewProgramMode"), "scModeButton");
	layout->addWidget(modeButton);
	settingsButton = MainButton(FrontendStr("Settings"), "scSettingsButton");
	layout->addWidget(settingsButton);

	layout->addStretch();

	/* stream-delay menu, attached to the stream button only while live with delay */
	streamMenu = new QMenu(this);
	stopStreamAction = streamMenu->addAction(PluginStr("SmallControls.StopStream"));
	forceStopStreamAction = streamMenu->addAction(FrontendStr("Basic.Main.ForceStopStreaming"));

	/* default visibility, as in the built-in dock */
	broadcastButton->setVisible(false);
	pauseButton->setVisible(false);
	replayButton->setVisible(false);
	saveReplayButton->setVisible(false);
	virtualCamButton->setVisible(false);
	virtualCamConfigButton->setVisible(false);

	connect(streamButton, &QPushButton::clicked, this, &SmallControls::OnStreamClicked);
	connect(stopStreamAction, &QAction::triggered, this, &SmallControls::OnStreamClicked);
	connect(forceStopStreamAction, &QAction::triggered, this, &SmallControls::OnForceStopStream);
	connect(broadcastButton, &QPushButton::clicked, this, &SmallControls::OnBroadcastClicked);
	connect(recordButton, &QPushButton::clicked, this, &SmallControls::OnRecordClicked);
	connect(pauseButton, &QPushButton::clicked, this, &SmallControls::OnPauseClicked);
	connect(replayButton, &QPushButton::clicked, this, &SmallControls::OnReplayClicked);
	connect(saveReplayButton, &QPushButton::clicked, this, &SmallControls::OnSaveReplayClicked);
	connect(virtualCamButton, &QPushButton::clicked, this, &SmallControls::OnVirtualCamClicked);
	connect(virtualCamConfigButton, &QPushButton::clicked, this, &SmallControls::OnVirtualCamConfigClicked);
	connect(modeButton, &QPushButton::clicked, this, &SmallControls::OnStudioModeClicked);
	connect(settingsButton, &QPushButton::clicked, this, &SmallControls::OnSettingsClicked);

	/* Constructed inside obs_module_load: no frontend queries until
	 * OBS_FRONTEND_EVENT_FINISHED_LOADING (see OBSFrontendEvent). */
	obs_frontend_add_event_callback(OBSFrontendEvent, this);
}

SmallControls::~SmallControls()
{
	obs_frontend_remove_event_callback(OBSFrontendEvent, this);
}

/* Main buttons may be narrower than their label. Keep the full text in a
 * property and show as much as fits with an ellipsis, instead of letting Qt
 * clip both ends of a centred label. */
void SmallControls::SetLabel(QPushButton *button, const QString &text)
{
	button->setProperty("fullText", text);
	button->setAccessibleName(text);
	button->setToolTip(text);
	ElideLabel(button);
}

/* The theme hands OBS its video-capture source icon through a dynamic
 * property on the main window; use that so the icon matches the theme.
 * Fall back to a bundled copy tinted with the button text colour. */
QIcon SmallControls::CameraIcon()
{
	if (QMainWindow *main = static_cast<QMainWindow *>(obs_frontend_get_main_window())) {
		QVariant v = main->property("cameraIcon");
		if (v.canConvert<QIcon>()) {
			QIcon icon = v.value<QIcon>();
			if (!icon.isNull())
				return icon;
		}
	}
	char *path = obs_module_file("icons/camera.svg");
	QIcon fallback;
	if (path) {
		QPixmap pm = QIcon(QString::fromUtf8(path)).pixmap(64, 64);
		QPainter p(&pm);
		p.setCompositionMode(QPainter::CompositionMode_SourceIn);
		p.fillRect(pm.rect(), QApplication::palette().color(QPalette::ButtonText));
		p.end();
		fallback = QIcon(pm);
		bfree(path);
	}
	return fallback;
}

void SmallControls::ElideLabel(QPushButton *button)
{
	QString full = button->property("fullText").toString();
	if (full.isEmpty()) {
		button->setText(QString());
		return;
	}
	QStyleOptionButton opt;
	opt.initFrom(button);
	QRect r = button->style()->subElementRect(QStyle::SE_PushButtonContents, &opt, button);
	int avail = (r.isValid() ? r.width() : button->width() - 12) - 4;
	if (avail <= 0)
		avail = button->width();

	bool fits = button->fontMetrics().horizontalAdvance(full) <= avail;

	/* Buttons flagged narrowIcon (Virtual Cam) drop the words entirely and
	 * show an icon once the label no longer fits. */
	if (!fits && button->property("narrowIcon").toBool()) {
		if (button->icon().isNull())
			button->setIcon(CameraIcon());
		if (!button->text().isEmpty())
			button->setText(QString());
		return;
	}
	if (!button->icon().isNull())
		button->setIcon(QIcon());

	QString shown = fits ? full : button->fontMetrics().elidedText(full, Qt::ElideRight, avail);
	if (button->text() != shown)
		button->setText(shown);
}

bool SmallControls::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::Resize || event->type() == QEvent::StyleChange ||
	    event->type() == QEvent::FontChange) {
		if (QPushButton *b = qobject_cast<QPushButton *>(watched))
			ElideLabel(b);
	}
	return QFrame::eventFilter(watched, event);
}

void SmallControls::showEvent(QShowEvent *event)
{
	QFrame::showEvent(event);
	ApplyUniformHeight();
}

/* In the built-in dock the Record / Replay / Virtual Camera rows come out
 * taller than Stream because the themed icon button beside them stretches
 * the row. Pin every button, icon buttons included, to the plain button
 * height so all six rows match. Re-run on theme changes, since the natural
 * height comes from the theme. */
void SmallControls::ApplyUniformHeight()
{
	/* let the stream button report its natural height again */
	streamButton->setMinimumHeight(0);
	streamButton->setMaximumHeight(QWIDGETSIZE_MAX);
	streamButton->ensurePolished();
	int h = streamButton->sizeHint().height();
	if (h <= 0)
		return;

	QPushButton *all[] = {streamButton,     broadcastButton,  recordButton,           pauseButton, replayButton,
			      saveReplayButton, virtualCamButton, virtualCamConfigButton, modeButton,  settingsButton};
	for (QPushButton *b : all)
		b->setFixedHeight(h);

	/* icon buttons: square, same height, never stretched by the layout */
	QPushButton *icons[] = {pauseButton, saveReplayButton, virtualCamConfigButton};
	for (QPushButton *b : icons)
		b->setFixedSize(h, h);
}

/* ------------------------------------------------------------------------- */
/* events                                                                    */

void SmallControls::OBSFrontendEvent(enum obs_frontend_event event, void *ptr)
{
	SmallControls *c = static_cast<SmallControls *>(ptr);

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		c->ready = true;
		c->UpdateReplayAvailability();
		c->UpdateVirtualCamAvailability();
		c->SyncAll();
		c->ApplyUniformHeight();
		break;
	case OBS_FRONTEND_EVENT_THEME_CHANGED:
		c->ApplyUniformHeight();
		c->virtualCamButton->setIcon(QIcon()); /* re-fetch the themed camera icon */
		ElideLabel(c->virtualCamButton);
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		c->UpdateReplayAvailability();
		c->SyncAll();
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		SetLabel(c->streamButton, FrontendStr("Basic.Main.Connecting"));
		c->SyncBroadcast();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		SetLabel(c->streamButton, PluginStr("SmallControls.Stopping"));
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		c->SyncStreaming();
		c->SyncBroadcast();
		break;

	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		SetLabel(c->recordButton, PluginStr("SmallControls.Stopping"));
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		c->SyncRecording();
		break;

	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING:
		SetLabel(c->replayButton, PluginStr("SmallControls.Stopping"));
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		c->SyncReplayBuffer();
		break;

	case OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED:
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED:
		c->SyncVirtualCam();
		break;

	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		c->SyncStudioMode();
		break;

	case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
	case OBS_FRONTEND_EVENT_EXIT:
		c->shuttingDown = true;
		break;
	default:
		break;
	}
}

/* ------------------------------------------------------------------------- */
/* state                                                                     */

QPushButton *SmallControls::BuiltinButton(const char *objectName)
{
	QMainWindow *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return main ? main->findChild<QPushButton *>(QString::fromUtf8(objectName)) : nullptr;
}

bool SmallControls::StreamDelayEnabled()
{
	config_t *config = obs_frontend_get_profile_config();
	return config && config_get_bool(config, "Output", "DelayEnable");
}

void SmallControls::SyncAll()
{
	if (!ready || shuttingDown)
		return;
	SyncStreaming();
	SyncRecording();
	SyncReplayBuffer();
	SyncVirtualCam();
	SyncStudioMode();
	SyncBroadcast();
}

void SmallControls::SyncStreaming()
{
	bool active = obs_frontend_streaming_active();
	streamButton->setEnabled(true);
	SetClasses(streamButton, active ? "state-active" : "");
	SetLabel(streamButton, PluginStr(active ? "SmallControls.StopStream" : "SmallControls.Stream"));

	/* With stream delay on, a click while live offers Stop vs Force Stop
	 * (discard delay), exactly like the built-in button. */
	if (active && StreamDelayEnabled())
		streamButton->setMenu(streamMenu);
	else
		streamButton->setMenu(nullptr);
}

void SmallControls::SyncRecording()
{
	bool active = obs_frontend_recording_active();
	SetClasses(recordButton, active ? "state-active" : "");
	SetLabel(recordButton, PluginStr(active ? "SmallControls.StopRecord" : "SmallControls.Record"));

	bool pausable = false;
	if (active) {
		obs_output_t *out = obs_frontend_get_recording_output();
		pausable = out && obs_output_can_pause(out);
		obs_output_release(out);
	}
	pauseButton->setVisible(pausable);

	if (pausable) {
		bool paused = obs_frontend_recording_paused();
		QString tip = FrontendStr(paused ? "Basic.Main.UnpauseRecording" : "Basic.Main.PauseRecording");
		SetClasses(pauseButton, paused ? "icon-media-pause state-active" : "icon-media-pause");
		pauseButton->setToolTip(tip);
		pauseButton->setAccessibleName(tip);
		saveReplayButton->setEnabled(!paused);
	} else {
		saveReplayButton->setEnabled(true);
	}
}

void SmallControls::SyncReplayBuffer()
{
	bool active = obs_frontend_replay_buffer_active();
	SetClasses(replayButton, active ? "state-active" : "");
	SetLabel(replayButton, PluginStr(active ? "SmallControls.StopReplay" : "SmallControls.ReplayBuffer"));
	saveReplayButton->setVisible(active && !replayButton->isHidden());
}

void SmallControls::SyncVirtualCam()
{
	bool active = obs_frontend_virtualcam_active();
	SetClasses(virtualCamButton, active ? "state-active" : "");
	SetLabel(virtualCamButton, PluginStr(active ? "SmallControls.StopVirtualCam" : "SmallControls.VirtualCam"));
}

void SmallControls::SyncStudioMode()
{
	SetClasses(modeButton, obs_frontend_preview_program_mode_active() ? "state-active" : "");
}

/* The YouTube broadcast flow lives entirely inside OBS's main window. Mirror
 * the built-in button's visibility, text and state, and forward clicks to it. */
void SmallControls::SyncBroadcast()
{
	QPushButton *b = BuiltinButton("broadcastButton");
	if (!b) {
		broadcastButton->setVisible(false);
		return;
	}
	broadcastButton->setVisible(!b->isHidden());
	broadcastButton->setEnabled(b->isEnabled());
	SetLabel(broadcastButton, b->text());
	broadcastButton->setProperty("broadcastState", b->property("broadcastState"));
	SetClasses(broadcastButton, b->property("class").toString());
}

/* Mirrors OBS: the replay buffer exists only when the active output mode has
 * it switched on (Simple and Advanced keep separate flags). */
void SmallControls::UpdateReplayAvailability()
{
	config_t *config = obs_frontend_get_profile_config();
	bool enabled = false;
	if (config) {
		const char *mode = config_get_string(config, "Output", "Mode");
		bool advanced = mode && strcmp(mode, "Advanced") == 0;
		enabled = config_get_bool(config, advanced ? "AdvOut" : "SimpleOutput", "RecRB");
		if (advanced) {
			const char *type = config_get_string(config, "AdvOut", "RecType");
			if (type && strcmp(type, "FFmpeg") == 0)
				enabled = false; /* custom FFmpeg output has no replay buffer */
		}
	}
	replayButton->setVisible(enabled);
	if (!enabled)
		saveReplayButton->setVisible(false);
}

void SmallControls::UpdateVirtualCamAvailability()
{
	bool available = (obs_get_output_flags(VIRTUAL_CAM_ID) & OBS_OUTPUT_VIDEO) != 0;
	virtualCamButton->setVisible(available);
	virtualCamConfigButton->setVisible(available);
}

/* ------------------------------------------------------------------------- */
/* actions                                                                   */

void SmallControls::OnStreamClicked()
{
	if (!ready || shuttingDown)
		return;
	if (obs_frontend_streaming_active())
		obs_frontend_streaming_stop();
	else
		obs_frontend_streaming_start();
}

void SmallControls::OnForceStopStream()
{
	if (!ready || shuttingDown)
		return;
	obs_output_t *out = obs_frontend_get_streaming_output();
	if (out)
		obs_output_force_stop(out);
	obs_output_release(out);
}

void SmallControls::OnRecordClicked()
{
	if (!ready || shuttingDown)
		return;
	if (obs_frontend_recording_active())
		obs_frontend_recording_stop();
	else
		obs_frontend_recording_start();
}

void SmallControls::OnPauseClicked()
{
	if (!ready || shuttingDown || !obs_frontend_recording_active())
		return;
	obs_frontend_recording_pause(!obs_frontend_recording_paused());
}

void SmallControls::OnReplayClicked()
{
	if (!ready || shuttingDown)
		return;
	if (obs_frontend_replay_buffer_active())
		obs_frontend_replay_buffer_stop();
	else
		obs_frontend_replay_buffer_start();
}

void SmallControls::OnSaveReplayClicked()
{
	if (!ready || shuttingDown)
		return;
	if (obs_frontend_replay_buffer_active())
		obs_frontend_replay_buffer_save();
}

void SmallControls::OnVirtualCamClicked()
{
	if (!ready || shuttingDown)
		return;
	if (obs_frontend_virtualcam_active())
		obs_frontend_stop_virtualcam();
	else
		obs_frontend_start_virtualcam();
}

void SmallControls::OnVirtualCamConfigClicked()
{
	if (!ready || shuttingDown)
		return;
	/* No public API opens the virtual camera dialog; press OBS's own button. */
	if (QPushButton *b = BuiltinButton("virtualCamConfigButton"))
		b->click();
}

void SmallControls::OnBroadcastClicked()
{
	if (!ready || shuttingDown)
		return;
	if (QPushButton *b = BuiltinButton("broadcastButton"))
		b->click();
}

void SmallControls::OnStudioModeClicked()
{
	if (!ready || shuttingDown)
		return;
	obs_frontend_set_preview_program_mode(!obs_frontend_preview_program_mode_active());
}

void SmallControls::OnSettingsClicked()
{
	if (!ready || shuttingDown)
		return;
	QMainWindow *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main)
		return;
	if (QAction *a = main->findChild<QAction *>("action_Settings")) {
		a->trigger();
		return;
	}
	if (QPushButton *b = BuiltinButton("settingsButton"))
		b->click();
}
