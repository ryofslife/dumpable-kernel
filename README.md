# dumpable-kernel

## As of 8/20/2023
I am trying to keep the number of online cores after the kexec reboot. 

---

I wanted to try out the kernel dump on RPI4 so I made some changes to the original source code found below.<br />
https://github.com/raspberrypi/linux <br />
You can compile the source code with the options below to enable the features necessary for kdump to work. Note that some of them might not be necessary if you only want to try the kexec reboot.<br />

<pre>
  CONFIG_DEBUG_KERNEL=y
  CONFIG_KEXEC=y　　　　　　　　　　　　
  CONFIG_KEXEC_FILE=y                   
  CONFIG_DEBUG_INFO=y　　　　　　　　　
  CONFIG_DEBUG_INFO_NONE=n                  
  CONFIG_CRASH_DUMP=y　　　　　　　　　　　　　
  CONFIG_PROC_VMCORE=y　　　　　　　　
  CONFIG_SMP=n　　　　　　　　　　　　　
  CONFIG_DEBUG_INFO_DWARF_TOOLCHAIN_DEFAULT=y     
  CONFIG_PROC_KCORE=y
</pre>

I also prepared a system call for triggering kernel panic. 
The system call number is 451. It takes an argument of 0 - 5. Here is some detail of what to expect for each case. 

---

### Case 0: <em>panic()</em>

If you pass 0 as the argument, it simply calls  <a href="https://github.com/ryofslife/dumpable-kernel/blob/main/kernel/panic.c#L284">panic()</a>. Nothing interesting but its useful for testing if kexec reboot is working properly😐.

---

### Case 1: Null Pointer Dereference

The 2nd option gives you the null pointer dereferencing error. The exception is raised by MMU by accessing <em>0000000000000000</em> as shown below.
<pre>
  [  916.176519] Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000  
</pre>
As it is triggered during the system call (el1), the el1h_64_sync handler will be called with the parameters describing the state of the ESR register shown below.
<pre>
   [  916.176597]   EC = 0x25: DABT (current EL), IL = 32 bits
   ...
   [  916.176645]   FSC = 0x05: level 1 translation fault
</pre>
The Exception Class(EC) is for classfying the error and is used for switching to the appropriate handler, which in this case is <a href="https://elixir.bootlin.com/linux/latest/source/arch/arm64/kernel/entry-common.c#L429">el1_abort</a>. 
<pre>
    crash> bt
  PID: 1238     TASK: ffffff8040b2be00  CPU: 1    COMMAND: "p4ni9_1"
   #0 [ffffffc0093037e0] machine_kexec at ffffffe8330353e8
   #1 [ffffffc009303810] __crash_kexec at ffffffe83315ec8c
   #2 [ffffffc0093039a0] panic at ffffffe833b911a4
   #3 [ffffffc009303a80] die at ffffffe833b9045c
   #4 [ffffffc009303b20] die_kernel_fault at ffffffe833b90a80
   #5 [ffffffc009303b70] __do_kernel_fault at ffffffe833037d10
   #6 [ffffffc009303bb0] do_page_fault at ffffffe833baddcc
   #7 [ffffffc009303c00] do_translation_fault at ffffffe833bae154
   #8 [ffffffc009303c20] do_mem_abort at ffffffe8330378a8
   #9 [ffffffc009303c50] el1_abort at ffffffe833b9dec0
  #10 [ffffffc009303c80] el1h_64_sync_handler at ffffffe833b9f154
  #11 [ffffffc009303dc0] el1h_64_sync at ffffffe83301127c
  #12 [ffffffc009303de0] __arm64_sys_p4ni9 at ffffffe8330aaffc
  #13 [ffffffc009303e00] invoke_syscall at ffffffe8330299dc
  #14 [ffffffc009303e30] el0_svc_common at ffffffe833029b18
  #15 [ffffffc009303e70] do_el0_svc at ffffffe833029c14
  #16 [ffffffc009303e80] el0_svc at ffffffe833b9eea0
  #17 [ffffffc009303ea0] el0t_64_sync_handler at ffffffe833b9f2f4
  #18 [ffffffc009303fe0] el0t_64_sync at ffffffe833011544
</pre>
Now let's take a look at how <a href="https://elixir.bootlin.com/linux/latest/source/arch/arm64/mm/fault.c#L842">do_mem_abort</a> calls <a href="https://elixir.bootlin.com/linux/latest/source/arch/arm64/mm/fault.c#L704">do_translation_fault</a> as shown above in the backtrace.<br />
When do_mem_abort gets called, it receives ESR. Then do_mem_abort uses the received ESR to refer to the <a href="https://elixir.bootlin.com/linux/latest/source/arch/arm64/mm/fault.c#L775">fault_info table</a> and calls the handler accordingly.<br />
For the rest, you can simply follow the function and should eventually end up panic() if you have <a href="https://elixir.bootlin.com/linux/v5.17.1/source/arch/arm64/kernel/traps.c#L232">CONFIG_PANIC_ON_OOPS enabled</a>😉.

---

### Case 2: Scheduling While Atomic

The third is my favorite😆, this case triggers 2 different errors and I'm still trying to understand it's behaviour. Most of the time it triggers translation fault, however, for once in a while, it results in scheduling error. Here I will explain whats happening with the latter case.<br />
If you take a look at the log, it gives you below explaning that preemption was disabled at the moment of scheduling. This is expected as spinlock disables preemption and the system call exits without releasing the lock. 
<pre>
   [  218.553759] BUG: scheduling while atomic: p4ni9_3/1134/0x00000002
   ...
   [  218.554068] Preemption disabled at:
   [  218.554073] [<ffffffda0a4ab0a4>] __arm64_sys_p4ni9+0x164/0x1b0
</pre>
Not releasing the spinlock will leave the task's preempt count > 0 as shown below, and triggers the warning <a href="https://elixir.bootlin.com/linux/v6.5.4/source/kernel/sched/core.c#L5961">"scheduling while atomic"</a>.
<pre>
   crash> bt
   PID: 1134     TASK: ffffff804a2c0000  CPU: 3    COMMAND: "p4ni9_3"
   ...
   crash> struct task_struct.thread_info.preempt.count ffffff804a2c0000
     thread_info.preempt.count = 3,
 crash>
</pre>
When I first encountered the error, I did some reserch and found there was this routine, <a href="https://elixir.bootlin.com/linux/v4.20.17/source/arch/arm64/kernel/entry.S#L914">work_pending</a>. It checks if there is a scheduled task waiting after the system call and my guess was that this routine is the one responsible for the warning.<br />
However, if you take a look at <a href="https://elixir.bootlin.com/linux/v6.5.5/source/arch/arm64/kernel/entry.S#L605">the recent version</a>, which is the one i'm using, you can see that the routine no longer exists🤔.<br />
So I came to the conclusion that the system timer is the one reponsible for the scheduling which explains the other occasional translation fault. Unlike the deterministic scheduling made by work_pending waiting at the exit of the system call, there is a chance of not going through scheduling with the system timer at the moment of exting the system call. 
Its fun to observe and study such behaviours😎.
