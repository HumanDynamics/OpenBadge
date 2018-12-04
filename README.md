# OpenBadge project #

Welcome to the Open Badge project, an open-source framework and toolkit for measuring and shaping face-to-face social interactions using either custom hardware devices or smart phones, and real-time web-based visualizations. Open Badges is a modular system that allows researchers to monitor and collect interaction data from people engaged in real-life social settings. For more information, please read our main publications ([1](https://ieeexplore.ieee.org/document/8259434),[2](https://www.media.mit.edu/publications/rhythm-a-unified-measurement-platform-for-human-organizations/),[3](https://www.media.mit.edu/publications/open-badges-a-low-cost-toolkit-for-measuring-team-communication-and-dynamics/)) ,visit our [wiki](https://github.com/HumanDynamics/OpenBadge/wiki), or join our [discussion forum](https://groups.google.com/d/forum/rhythm-badges).

Additional repositories that are part of this project:
* https://github.com/mitmedialab/rhythm-badge-programmer - schematics, vector files and instructions for making a programming rig 

* https://github.com/HumanDynamics/openbadge-hub-py - a python based hub (base station) for pulling data from badges

* https://github.com/HumanDynamics/openbadge-hub - a Javascript/Cordova app that communicates with the badges and stores
 the data to a server
* https://github.com/HumanDynamics/openbadge-server - the backend server for the app

![Badge](/images/badge_3v06.jpg?raw=true "Open Badge")

## Docker wrapper ##
The nRF working environment is now available as a Docker container. The following commands have been tested under Ubuntu
linux, but should work under iOS as well (with some modifications, especially to the device names).

Important notes:
1. The badge must be connected to the programmer before starting the docker (using docker-compose run)
2. Docker will mount the project directory under /app inside the container, so the container can see the latest changes
 (no need to build the container after every code change)

Commands:
* docker-compose build : builds the docker container "nrf". You must run this command at least once before calling
 docker-compose run.
* docker-compose run nrf <COMMAND> : where <COMMAND> is a command to run inside the container.
  * When calling make (e.g. make badge_03v6_noDebug flashUnlock flashErase  flashS130 flashAPP), the script will automatically
  change the working directory to the data_collector directory. You can see the different make options by calling:
  "docker-compose run nrf make"
* docker-compose run nrf bash : opens an interactive bash shell
* ./programming_loop_docker.sh [test/prod] : a utility used for programming a batch of badges. It will compile once, and
 then enters a loop. Each time a new badge is placed, it will be automatically programmed and its MAC address will be
 written to the macs.log file. For most purposes, use the "prod" command variables. The "test" option turns on serial
 output and other testing features and is not recommended for production.

If you are using a raspberry pi to run this code, please use the raspberry pi compose file (-f docker-compose-raspi.yml). For example `docker-compose -f docker-compose-raspi.yml build`.

## Firmware testing tools ##
The framework includes two tools for testing firmware code
* Badge terminal. The utility opens a connections to a badge and allows you to run simple commands. To run, use the following docker command: `docker-compose run nrf terminal <MAC ADDRESS>`
* Integration tests. These are tests designed for testing basic badge functionality. To run the tests, use the following docker command: `docker-compose run nrf run_all_tests <MAC ADDRESS>`

Note that in order to run the integration tests, you'll need to have a badge connected with a serial interface. A regular J-Link debugger/programmer does not include these. We recommend using a nRF51 dev-kit as a programmer for this purpose (see our wiki/documentations on how to make one).

