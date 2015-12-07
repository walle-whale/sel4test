

#include "../../test.h"

#include <sel4platsupport/plat/timer.h>


void 
plat_init_env(env_t env, test_init_data_t *data)
{
    env->timer = sel4platsupport_get_default_timer(&env->vka, &env->vspace,
                                                   &env->simple, env->timer_notification.cptr);
    if (env->timer == NULL) {
        ZF_LOGF("Failed to initialise default timer");
    }
    
    env->clock_timer = sel4platsupport_get_generic_timer();
    if (env->clock_timer == NULL) {
        ZF_LOGF("Failed to intiialise generic timer\n");
    }

    timer_start(env->clock_timer->timer);
}

seL4_Error
plat_get_frame_cap(void *data, void *paddr, int size_bits, cspacepath_t *path) 
{
    /* no frame cap for generic timer */
    return -1;
}

seL4_Error
plat_get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth)
{
    /* no irq for generic timer */
    return -1;
}

