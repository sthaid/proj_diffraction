// XXX test start pgm with voltage off

// XXX use the tic_settings pgm to update the settings
//       bring the settings directory to here
//       and command_timeout is 0,  OR maybe just longer

// XXX if an error occurs, such as losing voltage, need to recal
// XXX allow it to come up even if voltage not okay

// XXX log to file, and append

#include "common.h"

#include <tic.h>

//
// defines
//

// allowed voltage range, in mv
#define MIN_VIN_VOLTAGE 16000
#define MAX_VIN_VOLTAGE 24000

// these both return double, and can only be used when calibrated
#define CALIBRATED_MM_TO_POS(_mm)       (home_pos + (double)(_mm) * (200 * 32 / 5)) 
#define CALIBRATED_POS_TO_MM(_tgt_pos)  (((double)(_tgt_pos) - home_pos) / (200 * 32 / 5))

// execute tic routine and FATAL error if it fails
#define ERR_CHK(statement) \
    do { \
        tic_error * error; \
        error = statement; \
        if (error) { \
            FATAL("'%s' -> %s\n", #statement, tic_error_get_message(error)); \
        } \
    } while (0)

// expected value of command_timeout setting, 0 means disabled
#define COMMAND_TIMEOUT_MS 0

//
// typedes
//

//
// variables
//

static tic_handle * handle;
static bool         calibrated;
static int          tgt_pos;
static int          home_pos;

//
// prototypes
//

static void open_tic(void);
static void goto_tgt_pos(bool wait);
static bool at_tgt_pos(void);
static int get_curr_pos(void);
static void * keep_alive_thread(void * cx);
static void print_and_check_settings(void);
static void print_and_check_check_variables(void);
static bool is_status_okay(void);
static void get_status(bool *arg_okay, bool *arg_calibrated, double *arg_curr_loc_mm, 
                       char *arg_status_str, double *arg_voltage);
static char * operation_state_str(int op_state);
static char * error_status_str(int err_stat);

// -----------------  INIT / EXIT  ----------------------------------------

void xrail_init(void)
{
    pthread_t thread_id;
    int curr_pos;

    // get handle to the tic device
    open_tic();

    // reset the tic
    ERR_CHK(tic_reset(handle));
    ERR_CHK(tic_set_target_position(handle, 0));

    // print some settings and variables, and validate
    print_and_check_settings();
    print_and_check_check_variables();

    // create thread to periodically reset command timeout
    if (COMMAND_TIMEOUT_MS != 0) {
        pthread_create(&thread_id, NULL, keep_alive_thread, NULL);
    }

    // energize and exit_safe_start 
    ERR_CHK(tic_energize(handle));
    ERR_CHK(tic_exit_safe_start(handle));

    // confirm curret_position is 0
    curr_pos = get_curr_pos();
    if (curr_pos != 0) {
        FATAL("curr_pos=%d, should be 0\n", curr_pos);
    }
}

void xrail_exit(void)
{
    // if calibration is done then return home
    if (calibrated) {
        tgt_pos = home_pos;
        goto_tgt_pos(true);
    }

    // enter_safe_start and deenergize
    ERR_CHK(tic_enter_safe_start(handle));
    ERR_CHK(tic_deenergize(handle));

    // cleanup
    tic_handle_close(handle);
}

// -----------------  APIS  -----------------------------------------------

void xrail_cal_move(double mm)
{
    if (calibrated) {
        ERROR("already calibrated\n");
        return;
    }
    if (!is_status_okay()) {
        ERROR("not is_status_okay\n");
        return;
    }
    if (!at_tgt_pos()) {
        ERROR("not at_tgt_pos\n");
        return;
    }

    tgt_pos = get_curr_pos() + mm * (200 * 32 / 5);
    goto_tgt_pos(true);
}

void xrail_cal_complete(void)
{
    if (calibrated) {
        ERROR("already calibrated\n");
        return;
    }
    if (!is_status_okay()) {
        ERROR("not is_status_okay\n");
        return;
    }
    if (!at_tgt_pos()) {
        ERROR("not at_tgt_pos\n");
        return;
    }

    home_pos = get_curr_pos();
    calibrated = true;
}

void xrail_goto_location(double mm, bool wait)
{
    if (!calibrated) {
        ERROR("not calibrated\n");
        return;
    }

    tgt_pos = nearbyint(CALIBRATED_MM_TO_POS(mm));
    goto_tgt_pos(wait);
}

bool xrail_reached_location(void)
{
    if (!calibrated) {
        ERROR("not calibrated\n");
        return false;
    }

    return at_tgt_pos();
}

void xrail_get_status(bool *okay, bool *calibrated, double *curr_loc_mm, char *status_str, double *voltage)
{
    get_status(okay, calibrated, curr_loc_mm, status_str, voltage);
}

// -----------------  TIC PRIMARY SUPPORT ROUTINES  ----------------------

static void open_tic(void)
{
    tic_device ** list;
    size_t count;
    int i;

    // get list of connected devices;
    // this program supports just one device, so error if not 1 device
    ERR_CHK(tic_list_connected_devices(&list, &count));
    for (i = 0; i < count; i++) {
        INFO("device %d serial_number = %s\n", i, tic_device_get_serial_number(list[i]));
    }
    if (count != 1) {
        FATAL("must be just one device, but count=%zd\n", count);
    }

    // open handle to the tic device in list[0]
    ERR_CHK(tic_handle_open(list[0], &handle));

    // cleanup
    for (i = 0; i < count; i++) {
        tic_device_free(list[i]);
    }
    tic_list_free(list);
}

static void goto_tgt_pos(bool wait)
{
    ERR_CHK(tic_set_target_position(handle, tgt_pos));

    if (wait) {
        #define DELAY_US   100000
        #define TIMEOUT_US 5000000
        int curr_pos, usec=0;
        while (true) {
            curr_pos = get_curr_pos();
            if (curr_pos == tgt_pos) {
                break;
            }
            if (usec > TIMEOUT_US) {
                ERROR("failed to reach tgt_pos %d, curr_pos=%d\n", tgt_pos, curr_pos);
                calibrated = false;
                break;
            }
            usleep(DELAY_US);
            usec += DELAY_US;
        }
    }
}

static bool at_tgt_pos(void)
{
    return get_curr_pos() == tgt_pos;
}

static int get_curr_pos(void) 
{
    tic_variables *variables;
    int curr_pos;

    ERR_CHK(tic_get_variables(handle, &variables, false));
    curr_pos = tic_variables_get_current_position(variables);
    tic_variables_free(variables);

    return curr_pos;
}

static void * keep_alive_thread(void * cx)
{
    while (true) {
        ERR_CHK(tic_reset_command_timeout(handle));
        usleep(COMMAND_TIMEOUT_MS * 1000 / 2);
    }
    return NULL;
}

// -----------------  TIC INITIALIZATION CHECKS  -------------------------

static void print_and_check_settings(void)
{
    #define PRINT_SETTING(x) \
        do { \
            INFO("  %-32s = %d\n", #x, tic_settings_get_##x(settings)); \
        } while (0)

    tic_settings * settings = NULL;

    ERR_CHK(tic_get_settings(handle, &settings));

    INFO("SETTINGS ...\n");
    PRINT_SETTING(product);
    PRINT_SETTING(control_mode);
    PRINT_SETTING(never_sleep);
    PRINT_SETTING(disable_safe_start);
    PRINT_SETTING(ignore_err_line_high);
    PRINT_SETTING(auto_clear_driver_error);
    PRINT_SETTING(soft_error_response);
    PRINT_SETTING(soft_error_position);
    PRINT_SETTING(command_timeout);
    PRINT_SETTING(vin_calibration);
    PRINT_SETTING(current_limit);
    PRINT_SETTING(current_limit_code);
    PRINT_SETTING(current_limit_during_error);
    PRINT_SETTING(current_limit_code_during_error);
    PRINT_SETTING(step_mode);
    PRINT_SETTING(decay_mode);
    PRINT_SETTING(max_speed);
    PRINT_SETTING(starting_speed);
    PRINT_SETTING(max_accel);
    PRINT_SETTING(max_decel);
    PRINT_SETTING(invert_motor_direction);

    // Calculation of max_speed and max_accel register values ...
    //
    // NOTES:
    // - to accel to max speed in 1 secs ==> max_accel = max_speed / 100
    //
    // max_speed = 1 rev / sec
    //           = 200 * 32 * 10000
    //           = 64000000
    //
    // max_accel = accel to max speed in 1 sec
    //           = 64000000 / 100
    //           = 640000

    // check settings register values, and exit program if error; 
    // the settings should be programmed with ticgui

    #define CHECK_SETTING(x, expected_value) \
        do { \
            int actual_value; \
            actual_value = tic_settings_get_##x(settings); \
            if (actual_value != (expected_value)) { \
                FATAL("setting %s actual_value %d not equal expected %d\n", \
                       #x, actual_value, (expected_value)); \
            } \
        } while (0)

    CHECK_SETTING(control_mode, 0);                      // 0 => Serial/I2C/USB
    CHECK_SETTING(disable_safe_start, 0);                // 0 => safe start not disabled
    CHECK_SETTING(soft_error_response, 2);               // 2 => decel to hold
    CHECK_SETTING(command_timeout, COMMAND_TIMEOUT_MS);  // 
    CHECK_SETTING(current_limit, 1472);                  // 1472 ma
    CHECK_SETTING(current_limit_code_during_error, 255); // 0xff => feature disabled
    CHECK_SETTING(step_mode, 5);                         // 5 => 1/32 step
    CHECK_SETTING(decay_mode, 0);                        // 0 => mixed
    CHECK_SETTING(max_speed, 64000000);                  // 64000000 => shaft 360 deg/sec
    CHECK_SETTING(starting_speed, 0);                    // 0 => disallow instant accel or decel
    CHECK_SETTING(max_accel, 640000);                    // 640000 => accel to max speed in 1 secs
    CHECK_SETTING(max_decel, 0);                         // 0 => max_decel is same as max_accel
    CHECK_SETTING(invert_motor_direction,0);             // 0 => do not invert direction   

    tic_settings_free(settings);
    settings = NULL;
}

static void print_and_check_check_variables(void)
{
    #define PRINT_VARIABLE(x) \
        do { \
            INFO("  %-32s = %d\n", #x, tic_variables_get_##x(variables)); \
        } while (0)

    tic_variables *variables = NULL;

    ERR_CHK(tic_get_variables(handle, &variables, false));

    INFO("VARIABLES ...\n");
    PRINT_VARIABLE(operation_state);
    PRINT_VARIABLE(energized);
    PRINT_VARIABLE(position_uncertain);
    PRINT_VARIABLE(error_status);
    PRINT_VARIABLE(errors_occurred);
    PRINT_VARIABLE(planning_mode);
    PRINT_VARIABLE(target_position);
    PRINT_VARIABLE(target_velocity);
    PRINT_VARIABLE(max_speed);
    PRINT_VARIABLE(starting_speed);
    PRINT_VARIABLE(max_accel);
    PRINT_VARIABLE(max_decel);
    PRINT_VARIABLE(current_position);
    PRINT_VARIABLE(current_velocity);
    PRINT_VARIABLE(acting_target_position);
    PRINT_VARIABLE(time_since_last_step);
    PRINT_VARIABLE(device_reset);
    PRINT_VARIABLE(vin_voltage);
    PRINT_VARIABLE(up_time);
    PRINT_VARIABLE(step_mode);
    PRINT_VARIABLE(current_limit);
    PRINT_VARIABLE(current_limit_code);
    PRINT_VARIABLE(decay_mode);
    PRINT_VARIABLE(input_state);

    // check voltage
    int vin_voltage = tic_variables_get_vin_voltage(variables);
    if (vin_voltage < MIN_VIN_VOLTAGE || vin_voltage > MAX_VIN_VOLTAGE) {
        FATAL("variable vin_voltage=%d out of range %d to %d\n",
              vin_voltage, MIN_VIN_VOLTAGE, MAX_VIN_VOLTAGE);
    }

    tic_variables_free(variables);
    variables = NULL;
}

// -----------------  TIC GET STATUS  ------------------------------------

static bool is_status_okay(void)
{
    bool okay;

    get_status(&okay, NULL, NULL, NULL, NULL);
    return okay;
}

static void get_status(bool *arg_okay, bool *arg_calibrated, double *arg_curr_loc_mm, 
                       char *arg_status_str, double *arg_voltage)
{
    tic_variables *variables = NULL;
    bool okay;

    ERR_CHK(tic_get_variables(handle, &variables, false));

    int var_operation_state    = tic_variables_get_operation_state(variables);
    int var_energized          = tic_variables_get_energized(variables);
    int var_error_status       = tic_variables_get_error_status(variables);
    int var_vin_voltage        = tic_variables_get_vin_voltage(variables);
    int var_target_position    = tic_variables_get_target_position(variables);
    int var_current_position   = tic_variables_get_current_position(variables);

    tic_variables_free(variables);
    variables = NULL;

    okay = (var_target_position == tgt_pos &&
            var_operation_state == 10 &&
            var_energized == 1 &&
            var_error_status == 0 &&
            var_vin_voltage >= MIN_VIN_VOLTAGE && var_vin_voltage <= MAX_VIN_VOLTAGE);

    if (!okay && calibrated) {
        ERROR("clearing calibrated because of bad status\n");
        calibrated = false;
    }

    if (arg_okay) {
        *arg_okay = okay;
    }
    if (arg_calibrated) {
        *arg_calibrated = calibrated;
    }
    if (arg_curr_loc_mm) {
        *arg_curr_loc_mm = CALIBRATED_POS_TO_MM(var_current_position);
    }
    if (arg_status_str) {
        sprintf(arg_status_str, "%s - %s",
                operation_state_str(var_operation_state),
                error_status_str(var_error_status));
    }
    if (arg_voltage) {
        *arg_voltage = (double)var_vin_voltage / 1000;
    }
}

static char * operation_state_str(int op_state)
{
    switch (op_state) {
    case 0: return "RESET";
    case 2: return "DEENERGIZED";
    case 4: return "SOFT_ERR";
    case 6: return "WAITING_FOR_ERR_LINE";
    case 8: return "STARTING_UP";
    case 10: return "NORMAL";
    }
    return "INVALID";
}

static char * error_status_str(int err_stat) 
{
    static char str[100];
    char *p = str;

    if (err_stat == 0) return "No_Err";

    if (err_stat & (1<<0)) p += sprintf(p,"%s","Intentionally_de-energized,");
    if (err_stat & (1<<1)) p += sprintf(p,"%s","Motor_driver_error,");
    if (err_stat & (1<<2)) p += sprintf(p,"%s","Low_VIN,");
    if (err_stat & (1<<3)) p += sprintf(p,"%s","Kill_switch_active,");
    if (err_stat & (1<<4)) p += sprintf(p,"%s","Required_input_invalid,");
    if (err_stat & (1<<5)) p += sprintf(p,"%s","Serial_error,");
    if (err_stat & (1<<6)) p += sprintf(p,"%s","Command_timeout,");
    if (err_stat & (1<<7)) p += sprintf(p,"%s","Safe_start_violation,");
    if (err_stat & (1<<8)) p += sprintf(p,"%s","ERR_line_high,");

    if (p == str) return "Invalid_Err_Stat";

    return str;
}    

