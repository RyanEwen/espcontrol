#include <cstdlib>

#include "alarm_delay_audio.h"

int main() {
  if (alarm_delay_audio_mode_for_state("arming") != AlarmDelayAudioMode::EXIT) {
    return EXIT_FAILURE;
  }
  if (alarm_delay_audio_mode_for_state("pending") != AlarmDelayAudioMode::ENTRY) {
    return EXIT_FAILURE;
  }
  if (alarm_delay_audio_mode_for_state("triggered") != AlarmDelayAudioMode::NONE) {
    return EXIT_FAILURE;
  }
  if (!alarm_delay_audio_should_run("arming", 30, true, true) ||
      !alarm_delay_audio_should_run("pending", -1, true, true)) {
    return EXIT_FAILURE;
  }
  if (alarm_delay_audio_should_run("disarmed", 30, true, true) ||
      alarm_delay_audio_should_run("arming", 0, true, true) ||
      alarm_delay_audio_should_run("arming", 30, false, true) ||
      alarm_delay_audio_should_run("arming", 30, true, false)) {
    return EXIT_FAILURE;
  }
  if (alarm_delay_audio_beep_period_ms(11, 10) != 1000U ||
      alarm_delay_audio_beep_period_ms(10, 10) != 700U ||
      alarm_delay_audio_beep_period_ms(1, 10) != 700U ||
      alarm_delay_audio_beep_period_ms(-1, 10) != 1000U) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
