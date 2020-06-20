// audio.c
void audio_init(void);
void audio_exit(void);

void audio_say_text(char * fmt, ...) __attribute__ ((format (printf, 1, 2)));

void audio_change_volume(int delta, bool say);
void audio_set_volume(int volume, bool say);
int audio_get_volume(void);

void audio_toggle_mute(bool say);
void audio_set_mute(bool mute, bool say);
bool audio_get_mute(void);
