# CSSE4011 Prac 2

## Details

Name: Jacob Gregg
ID: 47430246

## Folder Structure

```
.
repo
├── Prac2
│   ├── CMakeLists.txt
│   ├── Kconfig
│   ├── README.md
│   ├── boards
│   │   └── disco_l475_iot1.overlay
│   ├── gui.py
│   ├── prj.conf
│   ├── prj_blk.conf
│   ├── prj_flash_load.conf
│   ├── src
│   │   └── main.c
│   └── stm32_blk.conf
├── README.md
├── include
│   ├── button.h
│   ├── cli.h
│   ├── htss21.h
│   ├── json.h
│   ├── lis3mdl.h
│   ├── littlefs.h
│   ├── lps22hb.h
│   └── rtc.h
└── lib
    ├── button.c
    ├── cli.c
    ├── htss21.c
    ├── json.c
    ├── lis3mdl.c
    ├── littlefs.c
    ├── lps22hb.c
    └── rtc.c  
```

## Design Tasks

### DT1

The RTC is initiallised, it can then be read and set by the user through shell commands.
The format is year:month:day hour:minute:second and other variables are not able to be updated.
By default the RTC is set to no daylight savings time.

### DT2

Three sensor files were created, with four measurements. Each Measuremnet follows the same set up.
The sensor is initiallised and a thread for that specific reading is setup. 
The thread waits for a semaphore to be given then finds the appropriate reading.
The reading is placed in a ring buffer to be read from the main control thread.
The sensor then waits on the semaphore again.

### DT3

The Shell is used to either read the sensor value or update the rtc.
If the rtc is selected to either 'r' or 'w' then the shell command simply calls the rtc library and updates accordingly.
For the sensor shell, based on the DID provided, the shell callback gives the DID through queue to the device thread.
The device thread will give a semaphore to the specific sensor thread.
After waiting 10ms to ensure ring buffer data the data is taken from the ring buffer.
The data is printed to the screen and the device thread waits on the queue, the shell exits the "sensor" command.

### DT4

Using the same principle as DT3, the control thread will get the sensor reading.
The 's' will set the active sample and stop set active sample to 0.
The control thread will ID which active sample is set and follow the semaphore and ring buffer principle.
The return of the ring buffer is given to the json function which will format the json output.
The output is then printed to the screen.

### DT5

The shell will take on the following commands:
- mkdir
- create file
- write to file
- read to file
- show contents of directory
- log sensor data to a file
- start a 5s constant sensor log
- stop the constant logging

The read file and ls requires that "/lfs" be included in the input string
The mkdir, create file, write to file and log sensor require the relative path from "/lfs/"

### DT7

The GUI will connect to the board based on which one you select.
The gui will send the serial command of "sample w x" and "sample s x" based on the selection.
One time series graph is then created whilst the GUI is running.
This means it will be discontinous if changing the sampling DID

## References
No references were used, API documentation, tutor assistance and zephyr samples were used