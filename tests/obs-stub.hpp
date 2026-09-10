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

#pragma once

#include <obs.h>
#include <obs-frontend-api.h>

#include <QMainWindow>

#include <map>
#include <string>
#include <vector>

/*
 * In-memory stand-in for the slice of libobs and obs-frontend-api that
 * SmallControls touches. Tests set state here, fire frontend events through
 * the callback the widget registered, and read back which frontend actions
 * the widget invoked. The stub never flips state on its own; a test that
 * wants "streaming started" sets `streaming` and fires the event, the same
 * way OBS delivers it.
 */
struct FakeObs {
	bool streaming = false;
	bool recording = false;
	bool recordingPaused = false;
	bool recordingPausable = true;
	bool replayBuffer = false;
	bool virtualCam = false;
	bool studioMode = false;
	uint32_t virtualCamFlags = OBS_OUTPUT_VIDEO;

	/* profile config, keyed "Section/Name"; missing keys read as unset */
	std::map<std::string, std::string> config;
	/* obs_frontend_get_locale_string; missing keys return nullptr */
	std::map<std::string, std::string> frontendText;

	QMainWindow *mainWindow = nullptr;
	obs_frontend_event_cb callback = nullptr;
	void *callbackData = nullptr;

	/* frontend actions the widget called, in order */
	std::vector<std::string> calls;
	int outputsAcquired = 0;
	int outputsReleased = 0;

	void fire(enum obs_frontend_event event);
	void reset();
};

FakeObs &fakeObs();
