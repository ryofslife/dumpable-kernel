# dumpable-kernel

## As of 8/20/2023
I am trying to keep the number of online cores after the kexec reboot. 

---

I wanted to try out the kernel dump on RPI4 so I made some changes to the original source code found below.<br />
https://github.com/raspberrypi/linux <br />
You can compile the source code with the options below, some of them might not be necessary, to enable the features necessary for kdump to work.<br />

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
The system call number is 451. It takes an argument of either 0 or 1. If you pass 0, it simply calles panic(). I'm still working on the arguments larger than 1...
The system call should be helpful for those who wants to test kexec on RPI4 as I found the tricks for triggering kernel panic online not working such as below.

<pre>
echo c > /proc/sysrq-trigger
</pre>
