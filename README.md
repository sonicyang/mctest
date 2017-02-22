Introduction:
======================================================================

Features:
======================================================================
1. Execute Motion algorithm in both user and kernel space
2. Provide a interface to trigger the motion execution
3. Provide a interface to retrieve testing result, including
 - mean, max, min
 - comprehensive test log

Supported platfrom
======================================================================
1. i.MX6Q Sabresd with Linux v4.4
1. Cyclone 5 SoC Development Kit with Linux v4.4

Compiling kernel mctest:
======================================================================
## Requirement:
 - gcc/g++ >= 4.9
 - make
 - python

## Building
### ARM
```
ARCH=arm CROSS_COMPILE=${YOUR_CROSS_COMPILE} \
    KDIR=${YOUR_KDIR_PATH} TARGET=kmod make
```
### X86_64
```
ARCH=x86  KDIR=${YOUR_KDIR_PATH} TARGET=kmod make
```

An binary file `build/kmod/drivers/mctest.ko` would be generated.

Testing kernel mctest:
======================================================================
1. insmod mctest.ko
2. enable mctest kernel trace log, and clean the buffer.
   ```
   echo 1 > /sys/kernel/debug/tracing/events/mctest/enable
   echo 0 > /sys/kernel/debug/tracing/trace
   ```
3. setup number of iteration mctest will execute, and trigger it
   ```
   echo 1000 > /sys/devices/virtual/misc/motion-ctrl/experiment/trigger
   ```
4. retrieve the statistic results
   ```
   cat /sys/devices/virtual/misc/motion-ctrl/experiment/result
   ```
5. retrieve the logs
   ```
   cat /sys/kernel/debug/tracing/trace
   ```

6. End the test:
    ```
    rmmod mctest
    ```


Compiling user mctest:
======================================================================
## Requirement:
 - gcc/g++ >= 4.9
 - make

## ARM
    ```
    ARCH=arm CROSS_COMPILE=${YOUR_CROSS_COMPILE} \
    TARGET=user make
    ```
## X86_64
    ```
    ARCH=x86 TARGET=user make
    ```

An binary file `build/user/drivers/mctest` would be generated.

Testing user kernel mctest:
======================================================================
1. ./mctest &
2. setup number of iteration mctest will execute, and trigger it
   ```
   echo 1000 > /tmp/motion/trigger
   ```
3. retrieve the statistic results
   ```
   cat  /tmp/motion/result/statistic_result
   ```
4. retrieve the logs
   ```
   cat  /tmp/motion/result/raw_result
   ```

5. stop mctest
   killall mctest


mctest release note:
======================================================================
Warning :
  1. only one instance of user mode mctest can be executing simultaneously
