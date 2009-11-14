/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "decoder_api.h"

#include <glib.h>
#include <mikmod.h>
#include <assert.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mikmod"

/* this is largely copied from alsaplayer */

#define MIKMOD_FRAME_SIZE	4096

static BOOL mod_mpd_Init(void)
{
	return VC_Init();
}

static void mod_mpd_Exit(void)
{
	VC_Exit();
}

static void mod_mpd_Update(void)
{
}

static BOOL mod_mpd_IsThere(void)
{
	return 1;
}

static char drv_name[] = "MPD";
static char drv_version[] = "MPD Output Driver v0.1";

#if (LIBMIKMOD_VERSION > 0x030106)
static char drv_alias[] = "mpd";
#endif

static MDRIVER drv_mpd = {
	NULL,
	drv_name,
	drv_version,
	0,
	255,
#if (LIBMIKMOD_VERSION > 0x030106)
	drv_alias,
#if (LIBMIKMOD_VERSION >= 0x030200)
	NULL,  /* CmdLineHelp */
#endif
	NULL,  /* CommandLine */
#endif
	mod_mpd_IsThere,
	VC_SampleLoad,
	VC_SampleUnload,
	VC_SampleSpace,
	VC_SampleLength,
	mod_mpd_Init,
	mod_mpd_Exit,
	NULL,
	VC_SetNumVoices,
	VC_PlayStart,
	VC_PlayStop,
	mod_mpd_Update,
	NULL,
	VC_VoiceSetVolume,
	VC_VoiceGetVolume,
	VC_VoiceSetFrequency,
	VC_VoiceGetFrequency,
	VC_VoiceSetPanning,
	VC_VoiceGetPanning,
	VC_VoicePlay,
	VC_VoiceStop,
	VC_VoiceStopped,
	VC_VoiceGetPosition,
	VC_VoiceRealVolume
};

static bool
mod_initMikMod(G_GNUC_UNUSED const struct config_param *param)
{
	static char params[] = "";

	md_device = 0;
	md_reverb = 0;

	MikMod_RegisterDriver(&drv_mpd);
	MikMod_RegisterAllLoaders();

	md_pansep = 64;
	md_mixfreq = 44100;
	md_mode = (DMODE_SOFT_MUSIC | DMODE_INTERP | DMODE_STEREO |
		   DMODE_16BITS);

	if (MikMod_Init(params)) {
		g_warning("Could not init MikMod: %s\n",
			  MikMod_strerror(MikMod_errno));
		return false;
	}

	return true;
}

static void mod_finishMikMod(void)
{
	MikMod_Exit();
}

static void
mod_decode(struct decoder *decoder, const char *path)
{
	char *path2;
	MODULE *handle;
	struct audio_format audio_format;
	float total_time = 0.0;
	int ret;
	float secPerByte;
	SBYTE buffer[MIKMOD_FRAME_SIZE];
	enum decoder_command cmd = DECODE_COMMAND_NONE;

	path2 = g_strdup(path);
	handle = Player_Load(path2, 128, 0);
	g_free(path2);

	if (handle == NULL) {
		g_warning("failed to open mod: %s\n", path);
		return;
	}

	/* Prevent module from looping forever */
	handle->loop = 0;

	audio_format_init(&audio_format, 44100, 16, 2);
	assert(audio_format_valid(&audio_format));

	secPerByte =
	    1.0 / ((audio_format.bits * audio_format.channels / 8.0) *
		   (float)audio_format.sample_rate);

	decoder_initialized(decoder, &audio_format, false, 0);

	Player_Start(handle);
	while (cmd == DECODE_COMMAND_NONE && Player_Active()) {
		ret = VC_WriteBytes(buffer, sizeof(buffer));
		total_time += ret * secPerByte;
		cmd = decoder_data(decoder, NULL, buffer, ret,
				   total_time, 0, NULL);
	}

	Player_Stop();
	Player_Free(handle);
}

static struct tag *modTagDup(const char *file)
{
	char *path2;
	struct tag *ret = NULL;
	MODULE *moduleHandle;
	char *title;

	path2 = g_strdup(file);
	moduleHandle = Player_Load(path2, 128, 0);
	g_free(path2);

	if (moduleHandle == NULL) {
		g_debug("Failed to open file: %s", file);
		return NULL;

	}
	Player_Free(moduleHandle);

	ret = tag_new();

	ret->time = 0;

	path2 = g_strdup(file);
	title = g_strdup(Player_LoadTitle(path2));
	g_free(path2);
	if (title)
		tag_add_item(ret, TAG_TITLE, title);

	return ret;
}

static const char *const modSuffixes[] = {
	"amf",
	"dsm",
	"far",
	"gdm",
	"imf",
	"it",
	"med",
	"mod",
	"mtm",
	"s3m",
	"stm",
	"stx",
	"ult",
	"uni",
	"xm",
	NULL
};

const struct decoder_plugin mikmod_decoder_plugin = {
	.name = "mikmod",
	.init = mod_initMikMod,
	.finish = mod_finishMikMod,
	.file_decode = mod_decode,
	.tag_dup = modTagDup,
	.suffixes = modSuffixes,
};
