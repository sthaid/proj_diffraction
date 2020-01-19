#include "common.h"

//
// defines
//

//
// typedefs
//

//
// variables
//

//
// prototypes
//

static void * calculate_screen_image_thread(void *cx);

// -----------------  CALCULATE_SCREEN_IMAGE  -----------------------------------

void calculate_screen_image(params_t *p)
{
    pthread_t thread_id;

    // if calculation in progress or already complete then just return
    if (p->screen) {
        return;
    }

    // set p->screen to (void*)1 to indicate the calculation is in progress,
    // and create a thread to do the calculation; when the thread completes it
    // will set p->screen equal to the result
    p->screen = (void*)1;
    pthread_create(&thread_id, NULL, calculate_screen_image_thread, p);
}

// -----------------  CALCULATE_SCREEN_IMAGE_THREAD  ----------------------------

static void * calculate_screen_image_thread(void *cx)
{
    params_t *p = cx;



    // XXX when done set p->screen

    return NULL;
}
