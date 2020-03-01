#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include <util_misc.h>
#include <util_geometry.h>

//
// defines
//

#define MAX_CONFIG 100
#define MAX_ELEMENT 10

//
// typedefs
//

struct photon_s;

typedef struct {
    char name[100];
    struct element_s {
        char name[100];
        plane_t plane;
        int next;
        int (*hndlr)(struct element_s *elem, struct photon_s *photon);
    } element[MAX_ELEMENT];
} sim_config_t;

//
// variables
//

sim_config_t   config[MAX_CONFIG];
int            max_config;
sim_config_t * current_config;

//
// prototypes
//

int sim_init(void);
void sim_select_config(int config_idx);
void sim_reset(void);
void sim_run(void);
void sim_stop(void);
bool sim_is_running(void);

int display_init(void);
int display_hndlr(void);



