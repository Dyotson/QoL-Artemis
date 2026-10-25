#include "MelodyPlayer.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <algorithm>

#include "Ringtones.inc"

static const Chirp AlarmTone[] = {
		{ 1047, 1047, 120 }, { 0, 0, 60 }, { 1319, 1319, 120 }, { 0, 0, 60 }, { 1568, 1568, 120 }, { 0, 0, 60 }, { 2093, 2093, 300 }, { 0, 0, 500 },
		{ 1047, 1047, 120 }, { 0, 0, 60 }, { 1319, 1319, 120 }, { 0, 0, 60 }, { 1568, 1568, 120 }, { 0, 0, 60 }, { 2093, 2093, 300 }, { 0, 0, 900 },
};

MelodyPlayer::MelodyPlayer(ChirpSystem& chirp) : Threaded("Melody", 2 * 1024, 6), chirp(chirp), wake(xSemaphoreCreateBinary()){
	start();
}

const Chirp* MelodyPlayer::ringtone(size_t& count){
	count = sizeof(SpaceHarrierRing) / sizeof(SpaceHarrierRing[0]);
	return SpaceHarrierRing;
}

const Chirp* MelodyPlayer::alarmTone(size_t& count){
	count = sizeof(AlarmTone) / sizeof(AlarmTone[0]);
	return AlarmTone;
}

void MelodyPlayer::play(const Chirp* mel, size_t cnt, bool loop){
	count = cnt;
	looping = loop;
	melody = mel;
	generation++;
	xSemaphoreGive(wake);
}

void MelodyPlayer::stop(){
	melody = nullptr;
	generation++;
	chirp.stop();
	xSemaphoreGive(wake);
}

bool MelodyPlayer::isPlaying() const{
	return melody.load() != nullptr;
}

void MelodyPlayer::loop(){
	xSemaphoreTake(wake, portMAX_DELAY);

	const uint32_t gen = generation;
	const Chirp* mel = melody;
	const size_t cnt = count;
	if(mel == nullptr || cnt == 0) return;

	size_t i = 0;
	uint32_t played = 0; // [ms] already played of tone i (tones may span chunks)
	while(gen == generation){
		// Gather up to ChunkMs worth of tones and hand them to the chirp system
		Sound chunk;
		uint32_t chunkMs = 0;
		while(i < cnt && chunkMs < ChunkMs){
			const uint32_t left = mel[i].duration - played;
			const uint32_t take = std::min(left, ChunkMs - chunkMs);
			chunk.push_back({ mel[i].startFreq, mel[i].endFreq, (uint16_t) take });
			chunkMs += take;
			played += take;
			if(played >= mel[i].duration){
				played = 0;
				i++;
			}
		}

		chirp.play(chunk);

		// Wait for the chunk to finish, or for stop()/play() to interrupt us
		if(xSemaphoreTake(wake, pdMS_TO_TICKS(chunkMs)) == pdTRUE){
			xSemaphoreGive(wake); // let the next loop() pick up the new request
			return;
		}

		if(i >= cnt){
			if(!looping) break;
			i = 0;
		}
	}

	if(gen == generation){
		melody = nullptr;
	}
}
