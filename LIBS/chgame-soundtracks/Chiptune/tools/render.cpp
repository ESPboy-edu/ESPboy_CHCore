// Exercise and render the real tracker and mixer on a host, without Arduino.
#include "Synth.h"
#include "src/Songs.h"
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <string>

static unsigned clockFraction = 0;
static void put16(FILE *f, unsigned n) { fputc(n & 255, f); fputc((n >> 8) & 255, f); }
static void put32(FILE *f, unsigned n) { put16(f, n); put16(f, n >> 16); }
static void wavHeader(FILE *f, unsigned samples) {
    fwrite("RIFF", 1, 4, f); put32(f, samples + 36); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 1); put32(f, chip::SAMPLE_RATE);
    put32(f, chip::SAMPLE_RATE); put16(f, 1); put16(f, 8);
    fwrite("data", 1, 4, f); put32(f, samples);
}
static uint8_t nextSample() {
    clockFraction += 1000;
    if (clockFraction >= chip::SAMPLE_RATE) {
        clockFraction -= chip::SAMPLE_RATE;
        chip::millisecond();
    }
    return chip::sample();
}
int main(int argc, char **argv) {
    assert(argc == 2);
    chip::begin();
    const std::string out = argv[1];
    for (unsigned song = 0; song < SONG_COUNT; ++song) {
        const Song &s = SONGS[song];
        for (unsigned leadOnly = 0; leadOnly < 2; ++leadOnly) {
            chip::play(song);
            clockFraction = 0;
            for (unsigned ch = 1; ch < 4; ++ch) chip::mute(ch, leadOnly);
            const std::string name = out + "/" + std::to_string(song) + (leadOnly ? "-lead.wav" : "-full.wav");
            FILE *f = fopen(name.c_str(), "wb"); assert(f);
            unsigned count = uint64_t(s.length) * 60 * chip::SAMPLE_RATE / (s.bpm * 24);
            wavHeader(f, count);
            for (unsigned i = 0; i < count; ++i) fputc(nextSample(), f);
            fclose(f);
        }
        // Dump actual tracker states through three loops for score verification.
        for (unsigned ch = 1; ch < 4; ++ch) chip::mute(ch, false);
        chip::play(song);
        FILE *trace = fopen((out + "/" + std::to_string(song) + ".tsv").c_str(), "w"); assert(trace);
        unsigned previous = ~0u;
        while (chip::ticks() < s.length * 3u) {
            if (chip::ticks() != previous) {
                previous = chip::ticks();
                fprintf(trace, "%u", previous);
                for (unsigned ch = 0; ch < 4; ++ch) {
                    auto v = chip::voice(ch);
                    fprintf(trace, "\t%u\t%u\t%u", v.note, v.volume, v.increment);
                }
                fputc('\n', trace);
            }
            chip::millisecond();
        }
        fclose(trace);
        unsigned tick = chip::ticks();
        chip::pause(true);
        for (unsigned i = 0; i < 5000; ++i) {
            chip::millisecond();
            assert(chip::sample() == 128);
        }
        assert(chip::ticks() == tick);
        chip::pause(false);
        for (unsigned i = 0; i < 1000; ++i) chip::millisecond();
        assert(chip::ticks() > tick);
        chip::setVolume(0);
        for (unsigned i = 0; i < 1000; ++i) assert(chip::sample() == 128);
        chip::setVolume(3);
    }
    // Long-delay VLE (65 + 128) regression: this used to move the caller's
    // pointer-to-pointer instead of its stream pointer on multi-byte values.
    extern uint16_t read_vle(const uint8_t **);
    const uint8_t bytes[] = {0x81, 0x00, 0x55};
    const uint8_t *p = bytes;
    assert(read_vle(&p) == 128 && p == bytes + 2);
    printf("PASS: %u tracks rendered; pause/resume, mute volume and long delays checked\n", SONG_COUNT);
}
