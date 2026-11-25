#include "sfx.h"

#include <cmath>
#include <cstdlib>
#include <vector>

namespace
{
	const int   kRate = 44100;
	Sound       gSounds[SFX_COUNT] = {};
	bool        gReady = false;

	// 生成一段波形：f0 → f1 的扫频，out 为 0 时切成噪声（打击感）
	Wave makeSweep(float seconds, float f0, float f1, float noiseMix, float gain)
	{
		int n = (int)(seconds * kRate);
		std::vector<short> buf((size_t)n);

		double phase = 0.0;
		for (int i = 0; i < n; i++)
		{
			float t = (float)i / (float)n;
			float freq = f0 + (f1 - f0) * t;
			phase += 2.0 * 3.14159265358979 * freq / kRate;

			float sine = (float)sin(phase);
			float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
			float s = sine * (1.0f - noiseMix) + noise * noiseMix;

			// 起音 - 衰减包络
			float env = (t < 0.02f) ? (t / 0.02f) : powf(1.0f - t, 2.0f);
			buf[i] = (short)(s * env * gain * 32767.0f);
		}

		Wave w = {};
		w.frameCount = (unsigned int)n;
		w.sampleRate = kRate;
		w.sampleSize = 16;
		w.channels = 1;

		size_t bytes = (size_t)n * sizeof(short);
		w.data = (void*)malloc(bytes);
		if (w.data) { memcpy(w.data, buf.data(), bytes); }
		return w;
	}

	void loadOne(int id, Wave w)
	{
		gSounds[id] = LoadSoundFromWave(w);
		UnloadWave(w);
	}
}

void sfxInit()
{
	if (gReady) { return; }

	if (!IsAudioDeviceReady())
	{
		// 没有声卡/耳机时静默降级：游戏照常玩
		return;
	}

	loadOne(SFX_SWING,   makeSweep(0.10f, 900.0f, 300.0f, 0.55f, 0.35f));
	loadOne(SFX_HIT,     makeSweep(0.13f, 220.0f, 110.0f, 0.70f, 0.45f));
	loadOne(SFX_PICKUP,  makeSweep(0.11f, 880.0f, 1500.0f, 0.00f, 0.30f));
	loadOne(SFX_DESCEND, makeSweep(0.35f, 700.0f, 200.0f, 0.15f, 0.35f));
	loadOne(SFX_DEATH,   makeSweep(0.55f, 400.0f, 80.0f,  0.40f, 0.40f));

	gReady = true;
}

void sfxPlay(int id, float volume)
{
	if (!gReady || id < 0 || id >= SFX_COUNT) { return; }
	if (volume <= 0.001f) { return; }

	SetSoundVolume(gSounds[id], volume);
	PlaySound(gSounds[id]);
}

void sfxShutdown()
{
	if (!gReady) { return; }
	for (int i = 0; i < SFX_COUNT; i++)
	{
		UnloadSound(gSounds[i]);
		gSounds[i] = Sound{};
	}
	gReady = false;
}
