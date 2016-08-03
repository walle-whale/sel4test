/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include <sel4utils/arch/util.h>

#include <vka/object.h>

#include "../test.h"
#include "../helpers.h"

enum {
    FAULT_DATA_READ_PAGEFAULT = 1,
    FAULT_DATA_WRITE_PAGEFAULT = 2,
    FAULT_INSTRUCTION_PAGEFAULT = 3,
    FAULT_BAD_SYSCALL = 4,
    FAULT_BAD_INSTRUCTION = 5,
};

#define BAD_VADDR 0xf123456C
#define GOOD_MAGIC 0x15831851
#define BAD_MAGIC ~GOOD_MAGIC
#define BAD_SYSCALL_NUMBER 0xc1

#define EXPECTED_BADGE 0xabababa

extern char read_fault_address[];
extern char read_fault_restart_address[];
static void __attribute__((noinline))
do_read_fault(void)
{
    int *x = (int*)BAD_VADDR;
    int val = BAD_MAGIC;
    /* Do a read fault. */
#if defined(CONFIG_ARCH_ARM)
    asm volatile (
        "mov r0, %[val]\n\t"
        "read_fault_address:\n\t"
        "ldr r0, [%[addrreg]]\n\t"
        "read_fault_restart_address:\n\t"
        "mov %[val], r0\n\t"
        : [val] "+r" (val)
        : [addrreg] "r" (x)
        : "r0"
    );
#elif defined(CONFIG_ARCH_X86)
    asm volatile (
        "mov %[val], %%eax\n\t"
        "read_fault_address:\n\t"
        "mov (%[addrreg]), %%eax\n\t"
        "read_fault_restart_address:\n\t"
        "mov %%eax, %[val]\n\t"
        : [val] "+r" (val)
        : [addrreg] "r" (x)
        : "eax"
    );
#else
#error "Unknown architecture."
#endif
    test_check(val == GOOD_MAGIC);
}

extern char write_fault_address[];
extern char write_fault_restart_address[];
static void __attribute__((noinline))
do_write_fault(void)
{
    int *x = (int*)BAD_VADDR;
    int val = BAD_MAGIC;
    /* Do a write fault. */
#if defined(CONFIG_ARCH_ARM)
    asm volatile (
        "mov r0, %[val]\n\t"
        "write_fault_address:\n\t"
        "str r0, [%[addrreg]]\n\t"
        "write_fault_restart_address:\n\t"
        "mov %[val], r0\n\t"
        : [val] "+r" (val)
        : [addrreg] "r" (x)
        : "r0"
    );
#elif defined(CONFIG_ARCH_X86)
    asm volatile (
        "mov %[val], %%eax\n\t"
        "write_fault_address:\n\t"
        "mov %%eax, (%[addrreg])\n\t"
        "write_fault_restart_address:\n\t"
        "mov %%eax, %[val]\n\t"
        : [val] "+r" (val)
        : [addrreg] "r" (x)
        : "eax"
    );
#else
#error "Unknown architecture."
#endif
    test_check(val == GOOD_MAGIC);
}

extern char instruction_fault_restart_address[];
static void __attribute__((noinline))
do_instruction_fault(void)
{
    int *x = (int*)BAD_VADDR;
    int val = BAD_MAGIC;
    /* Jump to a crazy address. */
#if defined(CONFIG_ARCH_ARM)
    asm volatile (
        "mov r0, %[val]\n\t"
        "blx %[addrreg]\n\t"
        "instruction_fault_restart_address:\n\t"
        "mov %[val], r0\n\t"
        : [val] "+r" (val)
        : [addrreg] "r" (x)
        : "r0", "lr"
    );
#elif defined(CONFIG_ARCH_X86)
    asm volatile (
        "mov %[val], %%eax\n\t"
        "instruction_fault_address:\n\t"
        "jmp *%[addrreg]\n\t"
        "instruction_fault_restart_address:\n\t"
        "mov %%eax, %[val]\n\t"
        : [val] "+r" (val)
        : [addrreg] "r" (x)
        : "eax"
    );
#else
#error "Unknown architecture."
#endif
    test_check(val == GOOD_MAGIC);
}

extern char bad_syscall_address[];
extern char bad_syscall_restart_address[];
static void __attribute__((noinline))
do_bad_syscall(void)
{
    int *x = (int*)BAD_VADDR;
    int val = BAD_MAGIC;
    /* Do an undefined system call. */
#if defined(CONFIG_ARCH_ARM)
    asm volatile (
        "mov r7, %[scno]\n\t"
        "mov r0, %[val]\n\t"
        "bad_syscall_address:\n\t"
        "svc %[scno]\n\t"
        "bad_syscall_restart_address:\n\t"
        "mov %[val], r0\n\t"
        : [val] "+r" (val)
        : [addrreg] "r" (x),
        [scno] "i" (BAD_SYSCALL_NUMBER)
        : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "memory", "cc"
    );
#elif defined(CONFIG_ARCH_X86_64)
    asm volatile (
        "movl   %[val], %%eax\n\t"
        "movq   %%rsp, %%rcx\n\t"
        "leaq   1f, %%rdx\n\t"
        "bad_syscall_address:\n\t"
        "1: \n\t"
        "sysenter\n\t"
        "bad_syscall_restart_address:\n\t"
        "movl   %%eax, %[val]\n\t"
        : [val] "+r" (val)
        : [addrreg] "r" (x),
        [scno] "i" (BAD_SYSCALL_NUMBER)
        : "rax", "rbx", "rcx", "rdx"
    );
#elif defined(CONFIG_ARCH_IA32)
    asm volatile (
        "mov %[scno], %%eax\n\t"
        "mov %[val], %%ebx\n\t"
        "mov %%esp, %%ecx\n\t"
        "leal 1f, %%edx\n\t"
        "bad_syscall_address:\n\t"
        "1:\n\t"
        "sysenter\n\t"
        "bad_syscall_restart_address:\n\t"
        "mov %%ebx, %[val]\n\t"
        : [val] "+r" (val)
        : [addrreg] "r" (x),
        [scno] "i" (BAD_SYSCALL_NUMBER)
        : "eax", "ebx", "ecx", "edx"
    );
#else
#error "Unknown architecture."
#endif
    test_check(val == GOOD_MAGIC);
}

extern char bad_instruction_address[];
extern char bad_instruction_restart_address[];
static seL4_Word bad_instruction_sp; /* To reset afterwards. */
static seL4_Word bad_instruction_cpsr; /* For checking against. */
static void __attribute__((noinline))
do_bad_instruction(void)
{
    int val = BAD_MAGIC;
    /* Execute an undefined instruction. */
#if defined(CONFIG_ARCH_ARM)
    asm volatile (
        /* Save SP */
        "str sp, [%[sp]]\n\t"

        /* Save CPSR */
        "mrs r0, cpsr\n\t"
        "str r0, [%[cpsr]]\n\t"

        /* Set SP to val. */
        "mov sp, %[valptr]\n\t"

        "bad_instruction_address:\n\t"
        ".word 0xe7f000f0\n\t" /* Guaranteed to be undefined by ARM. */
        "bad_instruction_restart_address:\n\t"
        :
        : [sp] "r" (&bad_instruction_sp),
        [cpsr] "r" (&bad_instruction_cpsr),
        [valptr] "r" (&val)
        : "r0", "memory"
    );
#elif defined(CONFIG_ARCH_X86_64)
    asm volatile (
        /* save RSP */
        "movq   %%rsp, (%[sp])\n\t"
        "pushf\n\t"
        "pop    %%rax\n\t"
        "movq   %%rax, (%[cpsr])\n\t"
        "movq   %[valptr], %%rsp\n\t"
        "bad_instruction_address:\n\t"
        "ud2\n\t"
        "bad_instruction_restart_address:\n\t"
        :
        : [sp] "r" (&bad_instruction_sp),
        [cpsr] "r" (&bad_instruction_cpsr),
        [valptr] "r" (&val)
        : "rax", "memory"
    );
#elif defined(CONFIG_ARCH_IA32)
    asm volatile (
        /* Save SP */
        "mov %%esp, (%[sp])\n\t"

        /* Save CPSR */
        "pushf\n\t"
        "pop %%eax\n\t"
        "mov %%eax, (%[cpsr])\n\t"

        /* Set SP to val. */
        "mov %[valptr], %%esp\n\t"

        "bad_instruction_address:\n\t"
        "ud2\n\t"
        "bad_instruction_restart_address:\n\t"
        :
        : [sp] "r" (&bad_instruction_sp),
        [cpsr] "r" (&bad_instruction_cpsr),
        [valptr] "r" (&val)
        : "eax", "memory"
    );
#else
#error "Unknown architecture."
#endif
    test_check(val == GOOD_MAGIC);
}

static void __attribute__((noinline))
set_good_magic_and_set_pc(seL4_CPtr tcb, seL4_Word new_pc)
{
    /* Set their register to GOOD_MAGIC and set PC past fault. */
    int error;
    seL4_UserContext ctx;
    error = seL4_TCB_ReadRegisters(tcb,
                                   false,
                                   0,
                                   sizeof(ctx) / sizeof(seL4_Word),
                                   &ctx);
    test_assert_fatal(!error);
#if defined(CONFIG_ARCH_ARM)
    test_assert_fatal(ctx.r0 == BAD_MAGIC);
    ctx.r0 = GOOD_MAGIC;
    ctx.pc = new_pc;
#elif defined(CONFIG_ARCH_X86_64)
    test_assert_fatal((int)ctx.rax == BAD_MAGIC);
    ctx.rax = GOOD_MAGIC;
    ctx.rip = new_pc;
#elif defined(CONFIG_ARCH_IA32)
    test_assert_fatal(ctx.eax == BAD_MAGIC);
    ctx.eax = GOOD_MAGIC;
    ctx.eip = new_pc;
#else
#error "Unknown architecture."
#endif
    error = seL4_TCB_WriteRegisters(tcb,
                                    false,
                                    0,
                                    sizeof(ctx) / sizeof(seL4_Word),
                                    &ctx);
    test_assert_fatal(!error);
}

static int
handle_fault(seL4_CPtr fault_ep, seL4_CPtr tcb, seL4_Word expected_fault,
             int badged_or_restart)
{
    seL4_MessageInfo_t tag;
    seL4_Word sender_badge = 0;
    seL4_Word badged = !!(badged_or_restart & 2);
    seL4_Word restart = !!(badged_or_restart & 1);

    tag = seL4_Recv(fault_ep, &sender_badge);

    if (badged) {
        test_check(sender_badge == EXPECTED_BADGE);
    } else {
        test_check(sender_badge == 0);
    }

    switch (expected_fault) {
    case FAULT_DATA_READ_PAGEFAULT:
        test_check(seL4_MessageInfo_get_label(tag) == seL4_Fault_VMFault);
        test_check(seL4_MessageInfo_get_length(tag) == seL4_VMFault_Length);
        test_check(seL4_GetMR(seL4_VMFault_IP) == (seL4_Word)read_fault_address);
        test_check(seL4_GetMR(seL4_VMFault_Addr) == BAD_VADDR);
        test_check(seL4_GetMR(seL4_VMFault_PrefetchFault) == 0);
        test_check(sel4utils_is_read_fault());

        /* Clear MRs to ensure they get repopulated. */
        seL4_SetMR(seL4_VMFault_Addr, 0);

        set_good_magic_and_set_pc(tcb, (seL4_Word)read_fault_restart_address);
        if (restart) {
            seL4_Reply(tag);
        }
        break;

    case FAULT_DATA_WRITE_PAGEFAULT:
        test_check(seL4_MessageInfo_get_label(tag) == seL4_Fault_VMFault);
        test_check(seL4_MessageInfo_get_length(tag) == seL4_VMFault_Length);
        test_check(seL4_GetMR(seL4_VMFault_IP) == (seL4_Word)write_fault_address);
        test_check(seL4_GetMR(seL4_VMFault_Addr) == BAD_VADDR);
        test_check(seL4_GetMR(seL4_VMFault_PrefetchFault) == 0);
        test_check(!sel4utils_is_read_fault());

        /* Clear MRs to ensure they get repopulated. */
        seL4_SetMR(seL4_VMFault_Addr, 0);

        set_good_magic_and_set_pc(tcb, (seL4_Word)write_fault_restart_address);
        if (restart) {
            seL4_Reply(tag);
        }
        break;

    case FAULT_INSTRUCTION_PAGEFAULT:
        test_check(seL4_MessageInfo_get_label(tag) == seL4_Fault_VMFault);
        test_check(seL4_MessageInfo_get_length(tag) == seL4_VMFault_Length);
        test_check(seL4_GetMR(seL4_VMFault_IP) == BAD_VADDR);
        test_check(seL4_GetMR(seL4_VMFault_Addr) == BAD_VADDR);
#if defined(CONFIG_ARCH_ARM)
        /* Prefetch fault is only set on ARM. */
        test_check(seL4_GetMR(seL4_VMFault_PrefetchFault) == 1);
#endif
        test_check(sel4utils_is_read_fault());

        /* Clear MRs to ensure they get repopulated. */
        seL4_SetMR(seL4_VMFault_Addr, 0);

        set_good_magic_and_set_pc(tcb, (seL4_Word)instruction_fault_restart_address);
        if (restart) {
            seL4_Reply(tag);
        }
        break;

    case FAULT_BAD_SYSCALL:
        test_eq(seL4_MessageInfo_get_label(tag), seL4_Fault_UnknownSyscall);
        test_eq(seL4_MessageInfo_get_length(tag), seL4_UnknownSyscall_Length);
        test_eq(seL4_GetMR(seL4_UnknownSyscall_FaultIP), (seL4_Word)bad_syscall_address);
        test_eq(seL4_GetMR(seL4_UnknownSyscall_Syscall), BAD_SYSCALL_NUMBER);
        seL4_SetMR(seL4_UnknownSyscall_FaultIP, (seL4_Word)bad_syscall_restart_address);
#if defined(CONFIG_ARCH_ARM)
        test_eq(seL4_GetMR(seL4_UnknownSyscall_R0), BAD_MAGIC);
        seL4_SetMR(seL4_UnknownSyscall_R0, GOOD_MAGIC);
#elif defined(CONFIG_ARCH_X86_64)
        test_eq((int)seL4_GetMR(seL4_UnknownSyscall_RBX, BAD_MAGIC));
        test_eq(seL4_GetMR(seL4_UnknwonSyscall_RIP), (seL4_Word)bad_syscall_restart_address);
        seL4_SetMR(seL4_UnknownSyscall_RBX, GOOD_MAGIC);
#elif defined(CONFIG_ARCH_IA32)
        test_eq(seL4_GetMR(seL4_UnknownSyscall_EBX), BAD_MAGIC);
        seL4_SetMR(seL4_UnknownSyscall_EBX, GOOD_MAGIC);
        /* Syscalls on ia32 seem to restart themselves with sysenter. */
#else
#error "Unknown architecture."
#endif

        /* Flag that the thread should be restarted. */
        if (restart) {
            seL4_MessageInfo_ptr_set_label(&tag, 0);
        } else {
            seL4_MessageInfo_ptr_set_label(&tag, 1);
        }
        seL4_Reply(tag);
        break;

    case FAULT_BAD_INSTRUCTION:
        test_check(seL4_MessageInfo_get_label(tag) == seL4_Fault_UserException);
        test_check(seL4_MessageInfo_get_length(tag) == seL4_UserException_Length);
        test_check(seL4_GetMR(0) == (seL4_Word)bad_instruction_address);
        seL4_Word *valptr = (seL4_Word*)seL4_GetMR(1);
        test_check((int)*valptr == BAD_MAGIC);
#if defined(CONFIG_ARCH_ARM)
        test_check(seL4_GetMR(2) == bad_instruction_cpsr);
        test_check(seL4_GetMR(3) == 0);
        test_check(seL4_GetMR(4) == 0);
#elif defined(CONFIG_ARCH_X86)
        /*
         * Curiously, the "resume flag" (bit 16) is set between the
         * undefined syscall and seL4 grabbing the tasks's flags. This only
         * happens on x86 hardware, but not qemu. Just ignore it when
         * checking flags.
         */
        seL4_Word mask_out = ~(1 << 16);
        test_check(((seL4_GetMR(2) ^ bad_instruction_cpsr) & mask_out) == 0);
#else
#error "Unknown architecture."
#endif

        *valptr = GOOD_MAGIC;
        seL4_SetMR(0, (seL4_Word)bad_instruction_restart_address);
        seL4_SetMR(1, bad_instruction_sp);

        /* Flag that the thread should be restarted. */
        if (restart) {
            seL4_MessageInfo_ptr_set_label(&tag, 0);
        } else {
            seL4_MessageInfo_ptr_set_label(&tag, 1);
        }

        seL4_Reply(tag);
        break;


    default:
        /* What? Why are we here? What just happened? */
        test_assert_fatal(0);
        break;
    }

    return 0;
}

static int
cause_fault(int fault_type)
{
    switch (fault_type) {
    case FAULT_DATA_READ_PAGEFAULT:
        do_read_fault();
        break;
    case FAULT_DATA_WRITE_PAGEFAULT:
        do_write_fault();
        break;
    case FAULT_INSTRUCTION_PAGEFAULT:
        do_instruction_fault();
        break;
    case FAULT_BAD_SYSCALL:
        do_bad_syscall();
        break;
    case FAULT_BAD_INSTRUCTION:
        do_bad_instruction();
        break;
    }

    return 0;
}

static int
test_fault(env_t env, int fault_type, bool inter_as)
{
    helper_thread_t handler_thread;
    helper_thread_t faulter_thread;
    int error;

    for (int restart = 0; restart <= 1; restart++) {
        for (int prio = 100; prio <= 102; prio++) {
            for (int badged = 0; badged <= 1; badged++) {
                seL4_Word handler_arg0, handler_arg1;
                /* The endpoint on which faults are received. */
                seL4_CPtr fault_ep = vka_alloc_endpoint_leaky(&env->vka);
                if (badged) {
                    seL4_CPtr badged_fault_ep = get_free_slot(env);
                    seL4_CapData_t cap_data;
                    cap_data = seL4_CapData_Badge_new(EXPECTED_BADGE);
                    cnode_mint(env, fault_ep, badged_fault_ep, seL4_AllRights, cap_data);

                    fault_ep = badged_fault_ep;
                }

                seL4_CPtr faulter_vspace, faulter_cspace;

                if (inter_as) {
                    create_helper_process(env, &faulter_thread);
                    create_helper_process(env, &handler_thread);

                    /* copy the fault endpoint to the handler */
                    cspacepath_t path;
                    vka_cspace_make_path(&env->vka,  fault_ep, &path);
                    handler_arg0 = sel4utils_copy_cap_to_process(&handler_thread.process, path);
                    assert(handler_arg0 != -1);

                    /* copy the fault tcb to the handler */
                    vka_cspace_make_path(&env->vka, faulter_thread.thread.tcb.cptr, &path);
                    handler_arg1 = sel4utils_copy_cap_to_process(&handler_thread.process, path);
                    assert(handler_arg1 != -1);

                    faulter_cspace = faulter_thread.process.cspace.cptr;
                    faulter_vspace = faulter_thread.process.pd.cptr;
                } else {
                    create_helper_thread(env, &faulter_thread);
                    create_helper_thread(env, &handler_thread);
                    faulter_cspace = env->cspace_root;
                    faulter_vspace = env->page_directory;
                    handler_arg0 = fault_ep;
                    handler_arg1 = faulter_thread.thread.tcb.cptr;
                }

                set_helper_priority(&handler_thread, 101);

                error = seL4_TCB_Configure(faulter_thread.thread.tcb.cptr,
                                           fault_ep, seL4_CapNull,
                                           seL4_Prio_new(prio, prio, seL4_MinCrit, seL4_MinCrit),
                                           faulter_thread.thread.sched_context.cptr,
                                           faulter_cspace,
                                           seL4_CapData_Guard_new(0, seL4_WordBits - env->cspace_size_bits),
                                           faulter_vspace, seL4_NilData,
                                           faulter_thread.thread.ipc_buffer_addr,
                                           faulter_thread.thread.ipc_buffer);
                test_assert(!error);

                start_helper(env, &handler_thread, (helper_fn_t) handle_fault,
                             handler_arg0, handler_arg1, fault_type, (badged << 1) | restart);
                start_helper(env, &faulter_thread, (helper_fn_t) cause_fault,
                             fault_type, 0, 0, 0);
                wait_for_helper(&handler_thread);

                if (restart) {
                    wait_for_helper(&faulter_thread);
                }

                cleanup_helper(env, &handler_thread);
                cleanup_helper(env, &faulter_thread);
            }
        }
    }

    return sel4test_get_result();
}

#ifndef CONFIG_FT

static int test_read_fault(env_t env)
{
    return test_fault(env, FAULT_DATA_READ_PAGEFAULT, false);
}
DEFINE_TEST(PAGEFAULT0001, "Test read page fault", test_read_fault)

static int test_write_fault(env_t env)
{
    return test_fault(env, FAULT_DATA_WRITE_PAGEFAULT, false);
}
DEFINE_TEST(PAGEFAULT0002, "Test write page fault", test_write_fault)

static int test_execute_fault(env_t env)
{
    return test_fault(env,  FAULT_INSTRUCTION_PAGEFAULT, false);
}
DEFINE_TEST(PAGEFAULT0003, "Test execute page fault", test_execute_fault)

#endif

static int test_bad_syscall(env_t env)
{
    return test_fault(env, FAULT_BAD_SYSCALL, false);
}
DEFINE_TEST(PAGEFAULT0004, "Test unknown system call", test_bad_syscall)

static int test_bad_instruction(env_t env)
{
    return test_fault(env, FAULT_BAD_INSTRUCTION, false);
}
DEFINE_TEST(PAGEFAULT0005, "Test undefined instruction", test_bad_instruction)

static int test_read_fault_interas(env_t env)
{
    return test_fault(env, FAULT_DATA_READ_PAGEFAULT, true);
}
DEFINE_TEST(PAGEFAULT1001, "Test read page fault (inter-AS)", test_read_fault_interas)

static int test_write_fault_interas(env_t env)
{
    return test_fault(env, FAULT_DATA_WRITE_PAGEFAULT, true);
}
DEFINE_TEST(PAGEFAULT1002, "Test write page fault (inter-AS)", test_write_fault_interas)

static int test_execute_fault_interas(env_t env)
{
    return test_fault(env, FAULT_INSTRUCTION_PAGEFAULT, true);
}
DEFINE_TEST(PAGEFAULT1003, "Test execute page fault (inter-AS)", test_execute_fault_interas)

static int test_bad_syscall_interas(env_t env)
{
    return test_fault(env, FAULT_BAD_SYSCALL, true);
}
DEFINE_TEST(PAGEFAULT1004, "Test unknown system call (inter-AS)", test_bad_syscall_interas)

#if 0
/* This test needs some work. */
static int test_bad_instruction_interas(env_t env)
{
    return test_fault(&env->vka, FAULT_BAD_INSTRUCTION, true);
}
DEFINE_TEST(PAGEFAULT1005, "Test undefined instruction (inter-AS)", test_bad_instruction_interas)
#endif

static void
timeout_fault_0001_fn(void)
{
    while (1);
}

int
test_timeout_fault(env_t env)
{
    seL4_CPtr endpoint;
    helper_thread_t helper;
    seL4_Word data = 1;
    seL4_MessageInfo_t info;

    endpoint = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &helper);
    set_helper_sched_params(env, &helper, 0.1 * US_IN_S, US_IN_S, data);
    set_helper_tfep(env, &helper, endpoint);
    start_helper(env, &helper, (helper_fn_t) timeout_fault_0001_fn, 0, 0, 0, 0);

    /* wait for timeout fault */
    info = seL4_Recv(endpoint, NULL);
    for (int i = 0; i < 10; i++) {
        test_eq(seL4_MessageInfo_get_length(info), seL4_TimeoutFault_Length);
        test_check(seL4_isTimeoutFault_tag(info));
        test_eq(seL4_GetMR(seL4_TimeoutFault_Data), data);
        info = seL4_ReplyRecv(endpoint, seL4_MessageInfo_new(0, 0, 0, 0), NULL);
    }

    return sel4test_get_result();
}
DEFINE_TEST(TEMPORALFAULT0001, "Test timeout fault", test_timeout_fault)

void
timeout_fault_server_fn(seL4_CPtr ep, pstimer_t *timer)
{
    /* signal to initialiser that we are done, and wait for a message from
     * the client */
    ZF_LOGD("Server signal recv");
    seL4_SignalRecv(ep, ep, NULL);
    uint64_t start = timer_get_time(timer);
    uint64_t end = start;
    /* spin, this will use up all of the clients budget */
    while (end - start < (NS_IN_S / 2)) {
        end = timer_get_time(timer);
    }
    /* we should not get here, as a timeout fault should have been raised
     * and the handler will reset us */
    ZF_LOGF("Should not get here");
}

static int
timeout_fault_client_fn(seL4_CPtr ep)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);
    while (1) {
        info = seL4_Call(ep, info);
        /* call should have failed, timeout fault handler will send a -1 */
        test_eq(seL4_GetMR(0), -1);
    }
    return 0;
}

static int
handle_timeout_fault(seL4_CPtr tfep, seL4_Word expected_badge, sel4utils_thread_t *server,
                      seL4_CPtr reply, sel4utils_checkpoint_t *cp, seL4_CPtr ep,
                      seL4_Word expected_data, env_t env)
{
    seL4_Word badge;
    seL4_MessageInfo_t info;
    int error;

    /* wait for timeout fault */
    ZF_LOGD("Wait for tf");
    info = seL4_Recv(tfep, &badge);
    test_eq(badge, expected_badge);
    test_check(seL4_isTimeoutFault_tag(info));
    test_eq(seL4_GetMR(seL4_TimeoutFault_Data), expected_data);
    test_eq(seL4_MessageInfo_get_length(info), seL4_TimeoutFault_Length);


    /* reply to client on behalf of server */
    error = cnode_swapTCBcaller(env, reply, &server->tcb);
    test_eq(error, seL4_NoError);
    seL4_SetMR(0, -1);
    seL4_Send(reply, info);

   size_t stack_size = (uintptr_t) cp->thread->stack_top - (uintptr_t) sel4utils_get_sp(cp->regs);
   memcpy((void *) sel4utils_get_sp(cp->regs), cp->stack, stack_size);

    /* restore server */
    ZF_LOGD("Restoring server");
    error = seL4_SchedContext_Bind(server->sched_context.cptr, server->tcb.cptr);
    test_eq(error, seL4_NoError);

    ZF_LOGD("Reply to server");
    info = seL4_TimeoutFaultReply_new(true, cp->regs, sizeof(seL4_UserContext)/sizeof(seL4_Word));
    /* reply, restoring server state, and wait for server to init */
    seL4_ReplyRecv(ep, info, NULL);

    error = seL4_SchedContext_Unbind(server->sched_context.cptr);
    test_eq(error, seL4_NoError);

    return 0;
}

static int
test_timeout_fault_in_server(env_t env)
{
    helper_thread_t client, server;
    seL4_CPtr tfep, ep;
    int error;
    seL4_Word client_data = 1;
    seL4_Word server_badge = 2;
    sel4utils_checkpoint_t cp;

    tfep = vka_alloc_endpoint_leaky(&env->vka);
    ep = vka_alloc_endpoint_leaky(&env->vka);

    /* create the server */
    error = create_passive_thread_with_tfep(env, &server, tfep, server_badge,
                                            (helper_fn_t) timeout_fault_server_fn, ep,
                                            (seL4_Word) env->clock_timer->timer, 0, 0, &cp);
    test_eq(error, 0);

    /* create the client */
    create_helper_thread(env, &client);
    set_helper_sched_params(env, &client, 0.1 * US_IN_S, US_IN_S, client_data);
    start_helper(env, &client, (helper_fn_t) timeout_fault_client_fn, ep, 0, 0, 0);

    seL4_CPtr reply = get_free_slot(env);
    /* handle a few faults */
    for (int i = 0; i < 5; i++) {
        ZF_LOGD("Handling fault");
        error = handle_timeout_fault(tfep, server_badge, &server.thread, reply, &cp, ep,
                                      client_data, env);
        test_eq(error, 0);
    }

    return sel4test_get_result();

}
DEFINE_TEST(TEMPORALFAULT0002, "Handle a timeout fault in a server",
            test_timeout_fault_in_server)

static void
timeout_fault_proxy_fn(seL4_CPtr in, seL4_CPtr out)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 1);
    info = seL4_SignalRecv(in, in, NULL);
    while (1) {
        info = seL4_Call(out, info);
        seL4_ReplyRecv(in, info, NULL);
    }
}

static int
test_timeout_fault_nested_servers(env_t env)
{
    helper_thread_t client, server, proxy;
    seL4_CPtr client_proxy_ep, proxy_server_ep, tfep;
    int error;
    seL4_Word client_data, server_badge, proxy_badge;
    sel4utils_checkpoint_t proxy_cp, server_cp;
   
    client_data = 1;
    server_badge = 2;
    proxy_badge = 3;

    client_proxy_ep = vka_alloc_endpoint_leaky(&env->vka);
    proxy_server_ep = vka_alloc_endpoint_leaky(&env->vka);
    tfep = vka_alloc_endpoint_leaky(&env->vka);

    /* create server */
    error = create_passive_thread_with_tfep(env, &server, tfep, server_badge,
                                            (helper_fn_t) timeout_fault_server_fn, proxy_server_ep,
                                            (seL4_Word) env->clock_timer->timer, 0, 0, &server_cp);
    test_eq(error, 0);

    /* create proxy */
    error = create_passive_thread_with_tfep(env, &proxy, tfep, proxy_badge,
                                            (helper_fn_t) timeout_fault_proxy_fn, client_proxy_ep,
                                            proxy_server_ep, 0, 0, &proxy_cp);
    test_eq(error, 0);

    /* create client */
    create_helper_thread(env, &client);
    set_helper_sched_params(env, &client, 0.1 * US_IN_S, US_IN_S, client_data);
    start_helper(env, &client, (helper_fn_t) timeout_fault_client_fn, client_proxy_ep, 0, 0, 0);

    /* handle some faults */
    seL4_CPtr reply = get_free_slot(env);
    for (int i = 0; i < 5; i++) {
        /* server fault */
        error = handle_timeout_fault(tfep, server_badge, &server.thread, reply, &server_cp,
                                      proxy_server_ep, client_data, env);
        test_eq(error, 0);

        /* proxy fault */
        error = handle_timeout_fault(tfep, proxy_badge, &proxy.thread, reply, &proxy_cp,
                                      client_proxy_ep, client_data, env);
        test_eq(error, 0);
    }

    return sel4test_get_result();
}
DEFINE_TEST(TEMPORALFAULT0003, "Nested timeout fault", test_timeout_fault_nested_servers)

