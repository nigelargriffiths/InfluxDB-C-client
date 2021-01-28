#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ic.h"

int main(int argc, char **argv)
{
	int count;
	int i = 10;
	char buf[300 +1];
	char myhostname[256 +1];

/* RANGE uses to generate some random numbers between the min and max inclusive */
#define RANGE(min,max) ((int)random() % ((max) - (min) +1)) + (min)

	srandom(getpid());

	ic_debug(1); /* maximum output */

	ic_influx_database("silver2", 8086, "ic");
	ic_influx_userpw("nigel", "secret");

/* Intitalise */

        /* get the local machine hostname */
	if( gethostname(myhostname, sizeof(myhostname)) == -1) {
		error("gethostname() failed");
	}
	snprintf(buf, 300, "host=%s", myhostname);
	ic_tags(buf);

/* Main capture loop - often data capture agents run until they are killed or the server reboots */

    for(count=0; count < 4; count++) {

/* Simple Measure */

	ic_measure("cpu");
	ic_long("user",    RANGE(20,70));
	ic_double("system",RANGE(2,5) * 3.142) ;
	if(RANGE(0,10) >6)
	    ic_string("status", "high");
	else
	    ic_string("status", "low");
	ic_measureend();

/* Measure with a single subsection - could be more */
 
	ic_measure("disks");
	for( i = 0; i<3; i++) {
		sprintf(buf, "sda%d",i);
		ic_sub(buf);
		ic_long("reads",    (long long)(i * RANGE(1,30)));
		ic_double("writes", (double)   (i * 3.142 * RANGE(1,30)));
		ic_subend();
	}
	ic_measureend();

/* Send all data in one packet to InfluxDB */

	ic_push();

/* Wait until we need to capture the data again */

	sleep(5); 	/* Typically, this would be 60 seconds */
    }
exit(0);
}
