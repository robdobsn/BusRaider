# BusRaider ESP32 Firmware using ESP IDF

### Build, Flash, Monitor

A single script ./build-flash-monitor.sh can be used. It is intended to be run in a WSL2 prompt and partly runs in linux (build) and partly Windows (flash and monitor)

To do the steps separately take a look at the script and unpick the parts you need.

The monitoring from the script is done with SerialMonitor.py but running "idf.py monitor" has advantages if you are getting crash bugs and want to see an annotated stack-dump, etc

### Configure the project

Configuration of the IDF is done using the sdkconfig.defaults file in the buildConfigs/BusRaider folder.

To make changes to this file, simply edit it and then delete sdkconfig and build - a new sdkconfig will be created from the standard IDF settings the changes from the sdkconfig.defaults file.

Don't run makemenuconfig or similar.
