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

#include "obs-stub.hpp"

#include <obs-module.h>
#include <util/config-file.h>

#include <cstdlib>
#include <cstring>
#include <fstream>

/* The real types are opaque; the widget only ever passes the pointers back. */
struct config_data {};
struct obs_output {};
struct obs_module {};

static FakeObs g_fake;
static config_data g_config;
static obs_output g_output;
static obs_module g_module;
static std::map<std::string, std::string> g_moduleText;

FakeObs &fakeObs()
{
	return g_fake;
}

void FakeObs::fire(enum obs_frontend_event event)
{
	if (callback)
		callback(event, callbackData);
}

void FakeObs::reset()
{
	*this = FakeObs();
}

/* Parse the shipped en-US.ini so the tests exercise the real strings. */
static void loadModuleText()
{
	if (!g_moduleText.empty())
		return;
	std::ifstream in(SMALL_CONTROLS_LOCALE_FILE);
	std::string line;
	while (std::getline(in, line)) {
		size_t eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		std::string key = line.substr(0, eq);
		std::string value = line.substr(eq + 1);
		if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
			value = value.substr(1, value.size() - 2);
		g_moduleText[key] = value;
	}
}

static void record(const char *call)
{
	g_fake.calls.push_back(call);
}

extern "C" {

/* obs-module.h: normally provided by OBS_DECLARE_MODULE / USE_DEFAULT_LOCALE */

const char *obs_module_text(const char *lookup_string)
{
	loadModuleText();
	auto it = g_moduleText.find(lookup_string);
	return it == g_moduleText.end() ? lookup_string : it->second.c_str();
}

obs_module_t *obs_current_module(void)
{
	return &g_module;
}

/* obs.h */

char *obs_find_module_file(obs_module_t *, const char *file)
{
	std::string path = std::string(SMALL_CONTROLS_DATA_DIR) + "/" + file;
	size_t size = path.size() + 1;
	char *out = static_cast<char *>(malloc(size));
	memcpy(out, path.c_str(), size);
	return out;
}

void bfree(void *ptr)
{
	free(ptr);
}

uint32_t obs_get_output_flags(const char *id)
{
	return strcmp(id, "virtualcam_output") == 0 ? g_fake.virtualCamFlags : 0;
}

bool obs_output_can_pause(const obs_output_t *)
{
	return g_fake.recordingPausable;
}

void obs_output_release(obs_output_t *output)
{
	if (output)
		g_fake.outputsReleased++;
}

void obs_output_force_stop(obs_output_t *)
{
	record("output_force_stop");
}

/* util/config-file.h */

const char *config_get_string(config_t *, const char *section, const char *name)
{
	auto it = g_fake.config.find(std::string(section) + "/" + name);
	return it == g_fake.config.end() ? nullptr : it->second.c_str();
}

bool config_get_bool(config_t *, const char *section, const char *name)
{
	const char *value = config_get_string(nullptr, section, name);
	return value && (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
}

/* obs-frontend-api.h */

const char *obs_frontend_get_locale_string(const char *string)
{
	auto it = g_fake.frontendText.find(string);
	return it == g_fake.frontendText.end() ? nullptr : it->second.c_str();
}

void *obs_frontend_get_main_window(void)
{
	return g_fake.mainWindow;
}

void obs_frontend_add_event_callback(obs_frontend_event_cb callback, void *private_data)
{
	g_fake.callback = callback;
	g_fake.callbackData = private_data;
}

void obs_frontend_remove_event_callback(obs_frontend_event_cb callback, void *private_data)
{
	if (g_fake.callback == callback && g_fake.callbackData == private_data) {
		g_fake.callback = nullptr;
		g_fake.callbackData = nullptr;
	}
}

config_t *obs_frontend_get_profile_config(void)
{
	return &g_config;
}

obs_output_t *obs_frontend_get_recording_output(void)
{
	g_fake.outputsAcquired++;
	return &g_output;
}

obs_output_t *obs_frontend_get_streaming_output(void)
{
	g_fake.outputsAcquired++;
	return &g_output;
}

bool obs_frontend_streaming_active(void)
{
	return g_fake.streaming;
}

bool obs_frontend_recording_active(void)
{
	return g_fake.recording;
}

bool obs_frontend_recording_paused(void)
{
	return g_fake.recordingPaused;
}

bool obs_frontend_replay_buffer_active(void)
{
	return g_fake.replayBuffer;
}

bool obs_frontend_virtualcam_active(void)
{
	return g_fake.virtualCam;
}

bool obs_frontend_preview_program_mode_active(void)
{
	return g_fake.studioMode;
}

void obs_frontend_streaming_start(void)
{
	record("streaming_start");
}

void obs_frontend_streaming_stop(void)
{
	record("streaming_stop");
}

void obs_frontend_recording_start(void)
{
	record("recording_start");
}

void obs_frontend_recording_stop(void)
{
	record("recording_stop");
}

void obs_frontend_recording_pause(bool pause)
{
	record(pause ? "recording_pause:1" : "recording_pause:0");
}

void obs_frontend_replay_buffer_start(void)
{
	record("replay_buffer_start");
}

void obs_frontend_replay_buffer_stop(void)
{
	record("replay_buffer_stop");
}

void obs_frontend_replay_buffer_save(void)
{
	record("replay_buffer_save");
}

void obs_frontend_start_virtualcam(void)
{
	record("start_virtualcam");
}

void obs_frontend_stop_virtualcam(void)
{
	record("stop_virtualcam");
}

void obs_frontend_set_preview_program_mode(bool enable)
{
	record(enable ? "set_preview_program_mode:1" : "set_preview_program_mode:0");
}

} /* extern "C" */
