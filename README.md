# upkeep
A tracking and reporting service for distributed IOT devices.  

### About
This service handles device registration and can notify you if those devices later go down, reboot, etc. It also serves up a web interface that can be used to monitor those devices and is updated in place via websockets.

### Building
See makefile

### Todo
- Make the constants in main.c modifiable via config file or environment vars
- Also package device IP address in uptime_entry_t and ultimately in database.
- Include a client lib
- Add the web management html etc. files that get served to this repo.
- Build out email support as a notification option?
