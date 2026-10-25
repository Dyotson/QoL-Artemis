#ifndef ARTEMIS_FIRMWARE_MELODYPLAYER_H
#define ARTEMIS_FIRMWARE_MELODYPLAYER_H

#include "ChirpSystem.h"
#include "Util/Threaded.h"
#include <atomic>
#include <cstddef>

/**
 * Plays melodies longer than ChirpSystem's 2 s limit (ringtone, alarms) by feeding
 * them in chunks, optionally looping until stopped.
 */
class MelodyPlayer : public Threaded {
public:
	explicit MelodyPlayer(ChirpSystem& chirp);

	/** Starts playing. The melody must stay valid while playing (static tables). */
	void play(const Chirp* melody, size_t count, bool loop);
	void stop();
	bool isPlaying() const;

	// Built-in melodies
	static const Chirp* ringtone(size_t& count);
	static const Chirp* alarmTone(size_t& count);

private:
	ChirpSystem& chirp;

	std::atomic<const Chirp*> melody{ nullptr };
	std::atomic<size_t> count{ 0 };
	std::atomic<bool> looping{ false };
	std::atomic<uint32_t> generation{ 0 };
	SemaphoreHandle_t wake;

	static constexpr uint32_t ChunkMs = 1800;

	void loop() override;
};


#endif //ARTEMIS_FIRMWARE_MELODYPLAYER_H
