# InfluxDB-C-client
Save stats from a C program to a InfluxDB database is a simple way. Only 12 function in total.

Currently, there is no official InfluxDB C language client library. 

Fortunately, I wanted to do exactly that for capturing Operating System performance statistics for AIX and Linux. This data capturing tool is called "njmon" and is open source on Sourceforge. So having worked out how and developing a small library of 12 functions for my use to make saving data simple, I thought I would share it. I hope it will prove useful for others.

The InfluxDB documentation for statistics in Line Protocol format is fine but there are vague HTTP requirements and you have to discover the details by many experiments. This library includes the network socket handling layer too, which can be tricky the first time.

See Article on the AIX pert Blog: https://www.ibm.com/support/pages/influxdb-c-client-library-capturing-statistics
