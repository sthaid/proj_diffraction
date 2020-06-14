// xrail.c
void xrail_init(void);
void xrail_exit(void);
void xrail_cal_move(double mm);
void xrail_cal_complete(void);
void xrail_goto_location(double mm, bool wait);
bool xrail_reached_location(void);
void xrail_get_status(bool *okay, bool *calibrated, double *curr_loc_mm, double *tgt_loc_mm, double *voltage, char *status_str);

