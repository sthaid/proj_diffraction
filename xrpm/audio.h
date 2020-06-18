// audio.c
void audio_init(void);
void audio_exit(void);

void audio_say_text(char * fmt, ...) __attribute__ ((format (printf, 1, 2)));

void audio_change_volume(int delta);
void audio_set_volume(int volume);
int audio_get_volume(void);

void audio_toggle_mute(void);
void audio_set_mute(bool mute);
bool audio_get_mute(void);
