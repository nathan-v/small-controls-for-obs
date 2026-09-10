/*
Small Controls for OBS
Copyright (C) 2026 Nathan V

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

#include "harness.hpp"
#include "obs-stub.hpp"
#include "small-controls.hpp"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>

namespace {

/* wide enough that every label fits without eliding */
const int WIDE = 320;

QString cls(const QWidget *w)
{
	return w->property("class").toString();
}

QIcon solidIcon(Qt::GlobalColor color)
{
	QPixmap pm(16, 16);
	pm.fill(color);
	return QIcon(pm);
}

QStringList calls()
{
	QStringList out;
	for (const std::string &c : fakeObs().calls)
		out << QString::fromStdString(c);
	return out;
}

/* Counts a button's clicked() signal, standing in for QSignalSpy. */
struct ClickCounter {
	int count = 0;
	explicit ClickCounter(QAbstractButton *b)
	{
		QObject::connect(b, &QAbstractButton::clicked, b, [this] { count++; });
	}
};

/*
 * One widget per test, shown at a comfortable width, with the fake OBS
 * seeded the way a Simple-output profile with the replay buffer on looks.
 * Nothing is "ready" until the test fires FINISHED_LOADING.
 *
 * The widget lives inside a wider host window rather than being a window of
 * its own: in OBS it is a dock child, and a real window manager (the Windows
 * platform, where obs-deps Qt has no offscreen plugin) refuses to shrink a
 * top-level window below its frame's minimum, which would defeat the narrow
 * layout tests.
 */
struct Fixture {
	QMainWindow *mainWindow = nullptr;
	QWidget *host = nullptr;
	SmallControls *controls = nullptr;

	Fixture()
	{
		FakeObs &obs = fakeObs();
		obs.reset();
		mainWindow = new QMainWindow;
		obs.mainWindow = mainWindow;
		obs.config = {{"Output/Mode", "Simple"}, {"SimpleOutput/RecRB", "true"}};
		obs.frontendText = {
			{"Basic.Main.Connecting", "Connecting"},
			{"Basic.Main.SetupBroadcast", "Set Up Broadcast"},
			{"Basic.Main.PauseRecording", "Pause Recording"},
			{"Basic.Main.UnpauseRecording", "Unpause Recording"},
			{"Basic.Main.ForceStopStreaming", "Force Stop Streaming"},
			{"Basic.TogglePreviewProgramMode", "Studio Mode"},
			{"Settings", "Settings"},
		};
		host = new QWidget;
		host->resize(400, 500);
		controls = new SmallControls(host);
		controls->setGeometry(0, 0, WIDE, 400);
		host->show();
		pump();
		CHECK(controls->isVisible());
	}

	~Fixture()
	{
		delete host; /* owns controls */
		delete mainWindow;
		fakeObs().reset();
	}

	QPushButton *button(const char *name) const
	{
		QPushButton *b = controls->findChild<QPushButton *>(QString::fromUtf8(name));
		CHECK_MSG(b != nullptr, QString::fromUtf8(name));
		return b;
	}

	void finishLoading() const { fakeObs().fire(OBS_FRONTEND_EVENT_FINISHED_LOADING); }

	void resizeTo(int width) const
	{
		controls->resize(width, 400);
		pump();
	}

	static void pump()
	{
		for (int i = 0; i < 5; i++)
			QApplication::processEvents();
	}
};

} // namespace

/* ------------------------------------------------------------------------- */

TEST(hiddenUntilReady)
{
	Fixture f;
	CHECK(f.button("scBroadcastButton")->isHidden());
	CHECK(f.button("scPauseButton")->isHidden());
	CHECK(f.button("scReplayButton")->isHidden());
	CHECK(f.button("scSaveReplayButton")->isHidden());
	CHECK(f.button("scVirtualCamButton")->isHidden());
	CHECK(f.button("scVirtualCamConfigButton")->isHidden());

	CHECK(!f.button("scStreamButton")->isHidden());
	CHECK(!f.button("scRecordButton")->isHidden());
	CHECK(!f.button("scModeButton")->isHidden());
	CHECK(!f.button("scSettingsButton")->isHidden());

	CHECK_EQ(f.button("scStreamButton")->text(), QStringLiteral("Stream"));
	CHECK_EQ(f.button("scRecordButton")->text(), QStringLiteral("Record"));
	CHECK_EQ(f.button("scModeButton")->text(), QStringLiteral("Studio Mode"));
	CHECK_EQ(f.button("scSettingsButton")->text(), QStringLiteral("Settings"));
}

TEST(clicksIgnoredBeforeReady)
{
	Fixture f;
	f.button("scStreamButton")->click();
	f.button("scRecordButton")->click();
	f.button("scPauseButton")->click();
	f.button("scReplayButton")->click();
	f.button("scSaveReplayButton")->click();
	f.button("scVirtualCamButton")->click();
	f.button("scModeButton")->click();
	CHECK(calls().isEmpty());
}

TEST(replayAvailabilityFromProfile)
{
	struct Row {
		const char *label;
		const char *mode;
		const char *simpleRB;
		const char *advRB;
		const char *recType;
		bool visible;
	};
	const Row rows[] = {
		{"simple on", "Simple", "true", "false", "Standard", true},
		{"simple off", "Simple", "false", "true", "Standard", false},
		{"advanced on", "Advanced", "false", "true", "Standard", true},
		{"advanced off", "Advanced", "true", "false", "Standard", false},
		{"advanced ffmpeg", "Advanced", "true", "true", "FFmpeg", false},
		{"mode unset", "", "true", "true", "Standard", true},
	};

	for (const Row &row : rows) {
		Fixture f;
		FakeObs &obs = fakeObs();
		obs.config.clear();
		if (*row.mode)
			obs.config["Output/Mode"] = row.mode;
		obs.config["SimpleOutput/RecRB"] = row.simpleRB;
		obs.config["AdvOut/RecRB"] = row.advRB;
		obs.config["AdvOut/RecType"] = row.recType;

		f.finishLoading();
		CHECK_MSG(!f.button("scReplayButton")->isHidden() == row.visible, QString::fromUtf8(row.label));
		CHECK(f.button("scSaveReplayButton")->isHidden()); /* buffer not running */

		/* a profile switch re-evaluates it */
		obs.config["SimpleOutput/RecRB"] = "false";
		obs.config["AdvOut/RecRB"] = "false";
		obs.fire(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
		CHECK_MSG(f.button("scReplayButton")->isHidden(), QString::fromUtf8(row.label));
	}
}

TEST(virtualCamAvailability)
{
	{
		Fixture f;
		fakeObs().virtualCamFlags = 0;
		f.finishLoading();
		CHECK(f.button("scVirtualCamButton")->isHidden());
		CHECK(f.button("scVirtualCamConfigButton")->isHidden());
	}
	{
		Fixture f;
		fakeObs().virtualCamFlags = OBS_OUTPUT_VIDEO;
		f.finishLoading();
		CHECK(!f.button("scVirtualCamButton")->isHidden());
		CHECK(!f.button("scVirtualCamConfigButton")->isHidden());
		CHECK_EQ(f.button("scVirtualCamButton")->text(), QStringLiteral("Virtual Cam"));
	}
}

TEST(streamingTransitions)
{
	Fixture f;
	FakeObs &obs = fakeObs();
	QPushButton *stream = f.button("scStreamButton");
	f.finishLoading();
	CHECK_EQ(stream->text(), QStringLiteral("Stream"));
	CHECK_EQ(cls(stream), QString());

	obs.fire(OBS_FRONTEND_EVENT_STREAMING_STARTING);
	CHECK_EQ(stream->text(), QStringLiteral("Connecting"));

	obs.streaming = true;
	obs.fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);
	CHECK_EQ(stream->text(), QStringLiteral("Stop Stream"));
	CHECK_EQ(cls(stream), QStringLiteral("state-active"));
	CHECK_EQ(stream->toolTip(), QStringLiteral("Stop Stream"));

	obs.fire(OBS_FRONTEND_EVENT_STREAMING_STOPPING);
	CHECK_EQ(stream->text(), QStringLiteral("Stopping..."));

	obs.streaming = false;
	obs.fire(OBS_FRONTEND_EVENT_STREAMING_STOPPED);
	CHECK_EQ(stream->text(), QStringLiteral("Stream"));
	CHECK_EQ(cls(stream), QString());
}

TEST(streamDelayMenu)
{
	Fixture f;
	FakeObs &obs = fakeObs();
	QPushButton *stream = f.button("scStreamButton");

	obs.config["Output/DelayEnable"] = "true";
	obs.streaming = true;
	f.finishLoading();
	CHECK(stream->menu() != nullptr);
	CHECK_EQ(stream->menu()->actions().size(), 2);
	CHECK_EQ(stream->menu()->actions().at(0)->text(), QStringLiteral("Stop Stream"));
	CHECK_EQ(stream->menu()->actions().at(1)->text(), QStringLiteral("Force Stop Streaming"));

	/* delay off: plain button again */
	obs.config["Output/DelayEnable"] = "false";
	obs.fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);
	CHECK(stream->menu() == nullptr);

	/* delay on but not live: no menu */
	obs.config["Output/DelayEnable"] = "true";
	obs.streaming = false;
	obs.fire(OBS_FRONTEND_EVENT_STREAMING_STOPPED);
	CHECK(stream->menu() == nullptr);
}

TEST(recordingAndPause)
{
	Fixture f;
	FakeObs &obs = fakeObs();
	QPushButton *record = f.button("scRecordButton");
	QPushButton *pause = f.button("scPauseButton");
	QPushButton *save = f.button("scSaveReplayButton");
	f.finishLoading();
	CHECK(pause->isHidden());

	obs.recording = true;
	obs.fire(OBS_FRONTEND_EVENT_RECORDING_STARTED);
	CHECK_EQ(record->text(), QStringLiteral("Stop Record"));
	CHECK_EQ(cls(record), QStringLiteral("state-active"));
	CHECK(!pause->isHidden());
	CHECK_EQ(pause->toolTip(), QStringLiteral("Pause Recording"));
	CHECK_EQ(cls(pause), QStringLiteral("icon-media-pause"));
	CHECK(save->isEnabled());

	obs.recordingPaused = true;
	obs.fire(OBS_FRONTEND_EVENT_RECORDING_PAUSED);
	CHECK_EQ(cls(pause), QStringLiteral("icon-media-pause state-active"));
	CHECK_EQ(pause->toolTip(), QStringLiteral("Unpause Recording"));
	CHECK(!save->isEnabled());

	obs.recordingPaused = false;
	obs.fire(OBS_FRONTEND_EVENT_RECORDING_UNPAUSED);
	CHECK_EQ(cls(pause), QStringLiteral("icon-media-pause"));
	CHECK(save->isEnabled());

	/* an output that cannot pause gets no pause button */
	obs.recordingPausable = false;
	obs.fire(OBS_FRONTEND_EVENT_RECORDING_STARTED);
	CHECK(pause->isHidden());

	obs.fire(OBS_FRONTEND_EVENT_RECORDING_STOPPING);
	CHECK_EQ(record->text(), QStringLiteral("Stopping..."));

	obs.recording = false;
	obs.fire(OBS_FRONTEND_EVENT_RECORDING_STOPPED);
	CHECK_EQ(record->text(), QStringLiteral("Record"));
	CHECK_EQ(cls(record), QString());
	CHECK(pause->isHidden());
}

TEST(replayBufferState)
{
	Fixture f;
	FakeObs &obs = fakeObs();
	QPushButton *replay = f.button("scReplayButton");
	QPushButton *save = f.button("scSaveReplayButton");
	f.finishLoading();
	CHECK(!replay->isHidden());
	CHECK(save->isHidden());
	CHECK_EQ(replay->text(), QStringLiteral("Replay Buffer"));

	obs.replayBuffer = true;
	obs.fire(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED);
	CHECK_EQ(replay->text(), QStringLiteral("Stop Replay"));
	CHECK_EQ(cls(replay), QStringLiteral("state-active"));
	CHECK(!save->isHidden());

	obs.fire(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING);
	CHECK_EQ(replay->text(), QStringLiteral("Stopping..."));

	obs.replayBuffer = false;
	obs.fire(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
	CHECK_EQ(replay->text(), QStringLiteral("Replay Buffer"));
	CHECK_EQ(cls(replay), QString());
	CHECK(save->isHidden());
}

TEST(studioMode)
{
	Fixture f;
	FakeObs &obs = fakeObs();
	QPushButton *mode = f.button("scModeButton");
	f.finishLoading();
	CHECK_EQ(cls(mode), QString());

	obs.studioMode = true;
	obs.fire(OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED);
	CHECK_EQ(cls(mode), QStringLiteral("state-active"));

	obs.studioMode = false;
	obs.fire(OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED);
	CHECK_EQ(cls(mode), QString());
}

TEST(clicksRouteToFrontend)
{
	Fixture f;
	FakeObs &obs = fakeObs();
	f.finishLoading();

	f.button("scStreamButton")->click();
	obs.streaming = true;
	f.button("scStreamButton")->click();
	CHECK_EQ(calls(), QStringList({"streaming_start", "streaming_stop"}));
	obs.calls.clear();

	f.button("scRecordButton")->click();
	obs.recording = true;
	f.button("scRecordButton")->click();
	f.button("scPauseButton")->click();
	obs.recordingPaused = true;
	f.button("scPauseButton")->click();
	CHECK_EQ(calls(), QStringList({"recording_start", "recording_stop", "recording_pause:1", "recording_pause:0"}));
	obs.calls.clear();

	/* pause does nothing when not recording */
	obs.recording = false;
	f.button("scPauseButton")->click();
	CHECK(calls().isEmpty());

	f.button("scReplayButton")->click();
	f.button("scSaveReplayButton")->click(); /* buffer inactive: ignored */
	obs.replayBuffer = true;
	f.button("scReplayButton")->click();
	f.button("scSaveReplayButton")->click();
	CHECK_EQ(calls(), QStringList({"replay_buffer_start", "replay_buffer_stop", "replay_buffer_save"}));
	obs.calls.clear();

	f.button("scVirtualCamButton")->click();
	obs.virtualCam = true;
	f.button("scVirtualCamButton")->click();
	CHECK_EQ(calls(), QStringList({"start_virtualcam", "stop_virtualcam"}));
	obs.calls.clear();

	f.button("scModeButton")->click();
	obs.studioMode = true;
	f.button("scModeButton")->click();
	CHECK_EQ(calls(), QStringList({"set_preview_program_mode:1", "set_preview_program_mode:0"}));
	obs.calls.clear();

	/* once shutdown starts, clicks are dropped */
	obs.fire(OBS_FRONTEND_EVENT_EXIT);
	f.button("scStreamButton")->click();
	f.button("scRecordButton")->click();
	CHECK(calls().isEmpty());
}

TEST(forceStopStream)
{
	Fixture f;
	FakeObs &obs = fakeObs();
	obs.config["Output/DelayEnable"] = "true";
	obs.streaming = true;
	f.finishLoading();

	QMenu *menu = f.button("scStreamButton")->menu();
	CHECK(menu != nullptr);
	menu->actions().at(0)->trigger();
	menu->actions().at(1)->trigger();
	CHECK_EQ(calls(), QStringList({"streaming_stop", "output_force_stop"}));
}

TEST(broadcastMirrorsBuiltin)
{
	Fixture f;
	FakeObs &obs = fakeObs();
	QPushButton *builtin = new QPushButton(QStringLiteral("Go Live"), f.mainWindow);
	builtin->setObjectName(QStringLiteral("broadcastButton"));
	builtin->setProperty("broadcastState", 2);
	builtin->setProperty("class", QStringLiteral("state-active"));
	builtin->setEnabled(false);

	QPushButton *mirror = f.button("scBroadcastButton");
	f.finishLoading();
	CHECK(!mirror->isHidden());
	CHECK_EQ(mirror->text(), QStringLiteral("Go Live"));
	CHECK(!mirror->isEnabled());
	CHECK_EQ(cls(mirror), QStringLiteral("state-active"));
	CHECK_EQ(mirror->property("broadcastState").toInt(), 2);

	builtin->hide();
	obs.fire(OBS_FRONTEND_EVENT_STREAMING_STOPPED);
	CHECK(mirror->isHidden());

	builtin->show();
	builtin->setEnabled(true);
	builtin->setText(QStringLiteral("Set Up Broadcast"));
	builtin->setProperty("class", QString());
	obs.streaming = true;
	obs.fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);
	CHECK(!mirror->isHidden());
	CHECK(mirror->isEnabled());
	CHECK_EQ(mirror->text(), QStringLiteral("Set Up Broadcast"));
	CHECK_EQ(cls(mirror), QString());

	ClickCounter clicked(builtin);
	mirror->click();
	CHECK_EQ(clicked.count, 1);
}

TEST(broadcastHiddenWithoutBuiltin)
{
	Fixture f;
	f.finishLoading();
	CHECK(f.button("scBroadcastButton")->isHidden());
	f.button("scBroadcastButton")->click(); /* must not crash */
}

TEST(virtualCamConfigForwards)
{
	Fixture f;
	QPushButton *builtin = new QPushButton(f.mainWindow);
	builtin->setObjectName(QStringLiteral("virtualCamConfigButton"));
	ClickCounter clicked(builtin);

	f.button("scVirtualCamConfigButton")->click(); /* not ready yet */
	CHECK_EQ(clicked.count, 0);

	f.finishLoading();
	f.button("scVirtualCamConfigButton")->click();
	CHECK_EQ(clicked.count, 1);
}

TEST(settingsTriggersAction)
{
	Fixture f;
	QAction *action = new QAction(f.mainWindow);
	action->setObjectName(QStringLiteral("action_Settings"));
	int triggered = 0;
	QObject::connect(action, &QAction::triggered, action, [&triggered] { triggered++; });

	QPushButton *builtin = new QPushButton(f.mainWindow);
	builtin->setObjectName(QStringLiteral("settingsButton"));
	ClickCounter clicked(builtin);

	f.finishLoading();
	f.button("scSettingsButton")->click();
	CHECK_EQ(triggered, 1);
	CHECK_EQ(clicked.count, 0); /* the action wins when both exist */
}

TEST(settingsFallsBackToBuiltinButton)
{
	Fixture f;
	QPushButton *builtin = new QPushButton(f.mainWindow);
	builtin->setObjectName(QStringLiteral("settingsButton"));
	ClickCounter clicked(builtin);

	f.finishLoading();
	f.button("scSettingsButton")->click();
	CHECK_EQ(clicked.count, 1);
}

/* The headline claim: every row visible, and the dock still fits under the
 * 150 px the built-in dock demands for a single button. */
TEST(dockCanBeNarrow)
{
	Fixture f;
	FakeObs &obs = fakeObs();
	obs.recording = true;
	obs.replayBuffer = true;
	f.finishLoading();
	obs.fire(OBS_FRONTEND_EVENT_RECORDING_STARTED);
	obs.fire(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED);
	CHECK(!f.button("scPauseButton")->isHidden());
	CHECK(!f.button("scSaveReplayButton")->isHidden());
	CHECK(!f.button("scVirtualCamConfigButton")->isHidden());

	Fixture::pump();
	const int minimum = f.controls->minimumSizeHint().width();
	CHECK_MSG(minimum < 150, QStringLiteral("minimum width is %1").arg(minimum));
}

TEST(labelsElideWhenNarrow)
{
	Fixture f;
	fakeObs().virtualCamFlags = 0; /* no icon column, so the dock can go very narrow */
	f.finishLoading();
	QPushButton *replay = f.button("scReplayButton");
	const QString full = QStringLiteral("Replay Buffer");
	CHECK_EQ(replay->text(), full);

	f.resizeTo(f.controls->minimumSizeHint().width());
	CHECK_MSG(replay->fontMetrics().horizontalAdvance(full) > replay->width(),
		  QStringLiteral("test font too small for the label to overflow a %1 px button").arg(replay->width()));
	CHECK(replay->text() != full);
	CHECK(replay->text().endsWith(QChar(0x2026)));
	CHECK_EQ(replay->toolTip(), full);
	CHECK_EQ(replay->accessibleName(), full);

	f.resizeTo(WIDE);
	CHECK_EQ(replay->text(), full);
}

TEST(virtualCamSwapsToIcon)
{
	Fixture f;
	QIcon themed = solidIcon(Qt::red);
	f.mainWindow->setProperty("cameraIcon", QVariant::fromValue(themed));
	f.finishLoading();
	QPushButton *cam = f.button("scVirtualCamButton");
	CHECK_EQ(cam->text(), QStringLiteral("Virtual Cam"));
	CHECK(cam->icon().isNull());

	f.resizeTo(f.controls->minimumSizeHint().width());
	CHECK(cam->text().isEmpty());
	CHECK(!cam->icon().isNull());
	CHECK_EQ(cam->icon().cacheKey(), themed.cacheKey());
	CHECK_EQ(cam->toolTip(), QStringLiteral("Virtual Cam"));

	/* a theme change re-fetches the icon */
	QIcon retheme = solidIcon(Qt::blue);
	f.mainWindow->setProperty("cameraIcon", QVariant::fromValue(retheme));
	fakeObs().fire(OBS_FRONTEND_EVENT_THEME_CHANGED);
	CHECK_EQ(cam->icon().cacheKey(), retheme.cacheKey());

	f.resizeTo(WIDE);
	CHECK_EQ(cam->text(), QStringLiteral("Virtual Cam"));
	CHECK(cam->icon().isNull());
}

TEST(rowsShareOneHeight)
{
	Fixture f;
	FakeObs &obs = fakeObs();
	obs.recording = true;
	obs.replayBuffer = true;
	f.finishLoading();
	obs.fire(OBS_FRONTEND_EVENT_RECORDING_STARTED);
	obs.fire(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED);
	Fixture::pump();

	const int h = f.button("scStreamButton")->height();
	CHECK(h > 0);
	const char *all[] = {"scRecordButton", "scReplayButton", "scVirtualCamButton", "scModeButton",
			     "scSettingsButton"};
	for (const char *name : all)
		CHECK_MSG(f.button(name)->height() == h, QString::fromUtf8(name));

	const char *icons[] = {"scPauseButton", "scSaveReplayButton", "scVirtualCamConfigButton"};
	for (const char *name : icons) {
		CHECK_MSG(f.button(name)->height() == h, QString::fromUtf8(name));
		CHECK_MSG(f.button(name)->width() == h, QString::fromUtf8(name));
	}
}

TEST(outputsAreReleased)
{
	Fixture f;
	FakeObs &obs = fakeObs();
	obs.recording = true;
	obs.streaming = true;
	obs.config["Output/DelayEnable"] = "true";
	f.finishLoading();
	obs.fire(OBS_FRONTEND_EVENT_RECORDING_STARTED);
	obs.fire(OBS_FRONTEND_EVENT_RECORDING_PAUSED);
	f.button("scStreamButton")->menu()->actions().at(1)->trigger();

	CHECK(obs.outputsAcquired > 0);
	CHECK_EQ(obs.outputsReleased, obs.outputsAcquired);
}

TEST(callbackRemovedOnDestroy)
{
	Fixture f;
	CHECK(fakeObs().callback != nullptr);
	delete f.controls;
	f.controls = nullptr;
	CHECK(fakeObs().callback == nullptr);
}
