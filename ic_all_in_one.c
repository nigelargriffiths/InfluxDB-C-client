/*
 * Influx C (ic) client for data capture
 * Developer: Nigel Griffiths.
 * (C) Copyright 2021 Nigel Griffiths

    This program is free software: you can redistribute it and/or modify
    it under the terms of the gnu general public license as published by
    the free software foundation, either version 3 of the license, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but without any warranty; without even the implied warranty of
    merchantability or fitness for a particular purpose.  see the
    gnu general public license for more details.

    You should have received a copy of the gnu general public license
    along with this program.  if not, see <http://www.gnu.org/licenses/>.

    Compile: cc ic.c -g -O3 -o ic
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <inttypes.h>
#include "ic.h"
#include <stdarg.h>
#include <assert.h>

#define DEBUG   if(debug)
#define MEGABYTE ( 1024 * 1024 ) /* USed as the default buffer sizes */

int debug = 0; /* 0=off, 1=on basic, 2=trace like output */

char influx_hostname[1024 + 1] = { 0 };/* details of the influxdb server or telegraf */
char influx_ip[16 + 1] = { 0 };
long influx_port = 0;

char influx_database[256+1];            /* the influxdb database  */
char influx_username[64+1];             /* optional for influxdb access */
char influx_password[64+1];             /* optional for influxdb access */

char *output; /* all the stats must fit in this buffer */
long output_size = 0;
long output_char = 0;
long auto_push_limit = 1 * MEGABYTE;


char *influx_tags; /* saved tags for every influxdb line protocol mesurement */

int subended = 0;               /* stop ic_subend and ic_measureend both enig the measure */
int first_sub = 0;              /* need to remove the ic_measure measure before adding ic_sub measure */
char saved_section[64];
char saved_sub[64];

int sockfd=-1;                  /* file desciptor for socket connection */
typedef unsigned char ic_charmap_t[256/8];

void error(const char *buf)
{
    fprintf(stderr, "error: \"%s\" errno=%d meaning=\"%s\"\n", buf, errno, strerror(errno));
    close(sockfd);
    sleep(2);                   /* this can help the socket close cleanly at the remote end */
    exit(1);
}

void ic_debug(int level)
{
        debug = level;
}

const static ic_charmap_t ic_cmap_esc_measuerment={
    // ", "
   [0x00]=0x00, [0x01]=0x00, [0x02]=0x00, [0x03]=0x00, [0x04]=0x01, [0x05]=0x10, [0x06]=0x00, [0x07]=0x00,
   [0x08]=0x00, [0x09]=0x00, [0x0a]=0x00, [0x0b]=0x00, [0x0c]=0x00, [0x0d]=0x00, [0x0e]=0x00, [0x0f]=0x00,
   [0x10]=0x00, [0x11]=0x00, [0x12]=0x00, [0x13]=0x00, [0x14]=0x00, [0x15]=0x00, [0x16]=0x00, [0x17]=0x00,
   [0x18]=0x00, [0x19]=0x00, [0x1a]=0x00, [0x1b]=0x00, [0x1c]=0x00, [0x1d]=0x00, [0x1e]=0x00, [0x1f]=0x00,
};
const static ic_charmap_t ic_cmap_esc_fieldkey_tagkey_tagvalue={
    //match " ,="
   [0x00]=0x00, [0x01]=0x00, [0x02]=0x00, [0x03]=0x00, [0x04]=0x01, [0x05]=0x10, [0x06]=0x00, [0x07]=0x20,
   [0x08]=0x00, [0x09]=0x00, [0x0a]=0x00, [0x0b]=0x00, [0x0c]=0x00, [0x0d]=0x00, [0x0e]=0x00, [0x0f]=0x00,
   [0x10]=0x00, [0x11]=0x00, [0x12]=0x00, [0x13]=0x00, [0x14]=0x00, [0x15]=0x00, [0x16]=0x00, [0x17]=0x00,
   [0x18]=0x00, [0x19]=0x00, [0x1a]=0x00, [0x1b]=0x00, [0x1c]=0x00, [0x1d]=0x00, [0x1e]=0x00, [0x1f]=0x00,
};
const static ic_charmap_t ic_cmap_esc_string_fieldvalue={
    // "\"\\"
    [0x00]=0x00, [0x01]=0x00, [0x02]=0x00, [0x03]=0x00, [0x04]=0x04, [0x05]=0x00, [0x06]=0x00, [0x07]=0x00,
    [0x08]=0x00, [0x09]=0x00, [0x0a]=0x00, [0x0b]=0x10, [0x0c]=0x00, [0x0d]=0x00, [0x0e]=0x00, [0x0f]=0x00,
    [0x10]=0x00, [0x11]=0x00, [0x12]=0x00, [0x13]=0x00, [0x14]=0x00, [0x15]=0x00, [0x16]=0x00, [0x17]=0x00,
    [0x18]=0x00, [0x19]=0x00, [0x1a]=0x00, [0x1b]=0x00, [0x1c]=0x00, [0x1d]=0x00, [0x1e]=0x00, [0x1f]=0x00,
};

const static ic_charmap_t ic_rfc3986={
   // "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghifklmnopqrstuvwxyz*-._"
   [0x00]=0x00, [0x01]=0x00, [0x02]=0x00, [0x03]=0x00, [0x04]=0x00, [0x05]=0x64, [0x06]=0xff, [0x07]=0x03,
   [0x08]=0xfe, [0x09]=0xff, [0x0a]=0xff, [0x0b]=0x87, [0x0c]=0xfe, [0x0d]=0xfb, [0x0e]=0xff, [0x0f]=0x07,
   [0x10]=0x00, [0x11]=0x00, [0x12]=0x00, [0x13]=0x00, [0x14]=0x00, [0x15]=0x00, [0x16]=0x00, [0x17]=0x00,
   [0x18]=0x00, [0x19]=0x00, [0x1a]=0x00, [0x1b]=0x00, [0x1c]=0x00, [0x1d]=0x00, [0x1e]=0x00, [0x1f]=0x00,
};
 
static inline int
ic_test_in_charmap(const ic_charmap_t charmap, unsigned char ordinal){
    return charmap[ordinal/8]&(1u<<(ordinal%8));
}

/* ic_urlencode() */
ssize_t
ic_urlencode(char *buf, size_t buf_len, const char*src){
    size_t opos=0, ipos=0;
    
    --buf_len; // reserve space for EOS

    while(opos+2 < buf_len && src[ipos] != '\0'){
        char c= src[ipos++];

        if(!ic_test_in_charmap(ic_rfc3986, c)){
            sprintf( buf+opos, "%%%02X", c);
            opos+=3;
        }else{
            buf[opos++]=c;
        }
    }
    buf[opos]='\0';

    assert(src[ipos] == '\0');

    return opos;
}

/* ic_escape_str_cmap() */
ssize_t
ic_escape_str_cmap(const ic_charmap_t charmap, char *buf, size_t buf_len, const char*src){
    size_t opos=0, ipos=0;
    
    --buf_len; // reserve space for EOS

    while(opos < buf_len && src[ipos] != '\0'){
        char c= src[ipos++];

        if (c == '\n' || iscntrl((unsigned)c)){
            c=' '; /* replace problem characters and with a space */
        }else if(ic_test_in_charmap(charmap, c)){
            buf[opos++]='\\';
        }
        buf[opos++]=c;
    }
    buf[opos]='\0';

    assert(src[ipos] == '\0');

    return opos;
}

/* ic_tags_escaped() arguments are the measurement tags for influddb, NULL terminated */
/* example: ic_tags_escaped("host", "vm1234", NULL)   note:the comma & hostname of the virtual machine sending the data */
/* complex: ic_tags_escaped("host", "lpar42", "serialnum", "987654", "arch", "power9", NULL) note:the comma separated list */
void ic_tags_escaped(const char*first, ...){
	va_list c_a;
	const char *n_a;
	char sep='\0';
	size_t pos=0;
	ssize_t ret;

    DEBUG fprintf(stderr,"%s(%s)\n", __func__,first);
    if( influx_tags == NULL ) {
        if( (influx_tags = (char *)malloc(MEGABYTE)) == (char *)NULL)
           error("failed to malloc() tags buffer");
    }

	va_start(c_a, first);

    for( n_a = first ; n_a && pos < MEGABYTE - 1; n_a = va_arg(c_a, char*) ) {
        switch(sep){
            case ',':
                influx_tags[pos++]=sep;
            case '\0':
                sep='=';
                break;
            case '=':
                influx_tags[pos++]=sep;
                sep=',';
                break;
        }
        ret  = ic_escape_str_cmap(ic_cmap_esc_fieldkey_tagkey_tagvalue, influx_tags + pos, MEGABYTE - 1 - pos, n_a);
        if(ret > 0){
            pos += ret;
        }
    }

	va_end(c_a);

    influx_tags[pos]='\0';

}
/* ic_tags() argument is the measurement tags for influddb */
/* example: "host=vm1234"   note:the comma & hostname of the virtual machine sending the data */
/* complex: "host=lpar42,serialnum=987654,arch=power9" note:the comma separated list */
void ic_tags(const char *t)     
{
    DEBUG fprintf(stderr,"ic_tags(%s)\n",t);
    if( influx_tags == NULL ) {
        if( (influx_tags = (char *)malloc(MEGABYTE)) == (char *)NULL)
           error("failed to malloc() tags buffer");
    }

    strncpy(influx_tags, t, MEGABYTE);
}

void ic_influx_database(const char *host, long port, const char *db) /* note: converts influxdb hostname to ip address */
{
        struct hostent *he;
        char errorbuf[1024 +1 ];

        influx_port = port;
        strncpy(influx_database,db,256);

        if(host[0] <= '0' && host[0] <='9') { 
                DEBUG fprintf(stderr,"ic_influx(ipaddr=%s,port=%ld,database=%s))\n",host,port,db);
                strncpy(influx_ip,host,16);
        } else {
                DEBUG fprintf(stderr,"ic_influx_by_hostname(host=%s,port=%ld,database=%s))\n",host,port,db);
                strncpy(influx_hostname,host,1024);
                if (isalpha(host[0])) {

                    he = gethostbyname(host);
                    if (he == NULL) {
                        sprintf(errorbuf, "influx host=%s to ip address convertion failed gethostbyname(), bailing out\n", host);
                        error(errorbuf);
                    }
                    /* this could return multiple ip addresses but we assume its the first one */
                    if (he->h_addr_list[0] != NULL) {
                        strcpy(influx_ip, inet_ntoa(*(struct in_addr *) (he->h_addr_list[0])));
                        DEBUG fprintf(stderr,"ic_influx_by_hostname hostname=%s converted to ip address %s))\n",host,influx_ip);
                    } else {
                        sprintf(errorbuf, "influx host=%s to ip address convertion failed (empty list), bailing out\n", host);
                        error(errorbuf);
                    }
                } else {
                    strcpy( influx_ip, host); /* perhaps the hostname is actually an ip address */
                }
        }
}

void ic_influx_userpw(const char *user, const char *pw)
{
        DEBUG fprintf(stderr,"ic_influx_userpw(username=%s,pssword=%s))\n",user,pw);
        strncpy(influx_username,user,64);
        strncpy(influx_password,pw,64);
}

/* ic_influx_url setup influx db config from url */
/* example http://host:port/db */
/* example http://user:pass@host:port/db */
void ic_influx_url(const char * influx_db){
    int ret;
    char errorbuf[1024 +1 ];
    char ic_username[64 + 1]={0};
    char ic_password[64 + 1]={0};
    char ic_hostname[1024 + 1];
    unsigned ic_port;
    char ic_db[256 + 1];
    ret=sscanf(influx_db, "http://%[^:]:%[^@]@%[^:]:%i/%s", ic_username, ic_password, ic_hostname, &ic_port, ic_db);
    if(ret != 5){
        ret=sscanf(influx_db, "http://%[^:]:%i/%s", ic_hostname, &ic_port, ic_db);
        if(ret != 3){
            sprintf(errorbuf, "Error parsing influx_db '%s'\n", influx_db);
            error(errorbuf);
        }
        ic_username[0]='\0';
        ic_password[0]='\0';
    }
    ic_influx_userpw(ic_username, ic_password);
    ic_influx_database(ic_hostname, ic_port, ic_db);
}


int create_socket()             /* returns 1 for error and 0 for ok */
{
    static struct sockaddr_in serv_addr;

    if(debug) DEBUG fprintf(stderr, "socket: trying to connect to \"%s\":%ld\n", influx_ip, influx_port);
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error("socket() call failed");
        return 0;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(influx_ip);
    serv_addr.sin_port = htons(influx_port);

    /* connect tot he socket offered by the web server */
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        DEBUG fprintf(stderr, " connect() call failed errno=%d", errno);
        return 0;
    }
    return 1;
}

void ic_check(long adding) /* Check the buffer space */
{
    if(output_char + (2*adding) >= output_size) { /* When near the end of the output buffer, extend it*/
        output_size += MEGABYTE;
        if( (output = (char *)realloc(output, output_size)) == (char *)NULL)
            error("failed to realloc() output buffer");
    }
}

void remove_ending_comma_if_any()
{
    if (output[output_char - 1] == ',') {
        output[output_char - 1] = 0;    /* remove the char */
        output_char--;
    }
}

void ic_measure(const char *section)
{
    ic_check( strlen(section) + strlen(influx_tags) + 3);

    ic_escape_str_cmap(ic_cmap_esc_measuerment, saved_section, sizeof(saved_section), section);

    output_char += sprintf(&output[output_char], "%s,%s ", saved_section, influx_tags);
    first_sub = 1;
    subended = 0;
    DEBUG fprintf(stderr, "ic_measure(\"%s\") count=%ld\n", section, output_char);
}

#define TIMESTAMP_STR_LEN sizeof("18446744073709551615")
void ic_measureend(const uint64_t stamp)
{
    
    ic_check( 4 + TIMESTAMP_STR_LEN);
    remove_ending_comma_if_any();
    if (!subended) {
        if(stamp){
            output_char += sprintf(&output[output_char], "  %"PRIu64"\n", stamp);
        }else{
         output_char += sprintf(&output[output_char], "   \n");
        }
    }
    subended = 0;
    DEBUG fprintf(stderr, "ic_measureend()\n");
    if(output_char > auto_push_limit){
        ic_push();
    }
}

/* Note this added a further tag to the measurement of the "resource_name" */
/* measurement might be "disks" */
/* sub might be "sda1", "sdb1", etc */
void ic_sub(const char *resource)
{
    char escaped_resource[256];
    long i;

    ic_escape_str_cmap(ic_cmap_esc_fieldkey_tagkey_tagvalue, escaped_resource, sizeof(escaped_resource), resource);

    ic_check( strlen(saved_section) + strlen(influx_tags) +strlen(saved_sub) + strlen(escaped_resource) + 9);

    /* remove previously added section */
    if (first_sub) {
        for (i = output_char - 1; i > 0; i--) {
            if (output[i] == '\n') {
                output[i + 1] = 0;
                output_char = i + 1;
                break;
            }
        }
        if(i == 0){
            output_char = 0;
        }
    }
    first_sub = 0;

    /* remove the trailing s */
    if(strlen(saved_section) > sizeof(saved_sub)){
        abort();
    }
    strcpy(saved_sub, saved_section);
    if (saved_sub[strlen(saved_sub) - 1] == 's') {
        saved_sub[strlen(saved_sub) - 1] = 0;
    }
    output_char += sprintf(&output[output_char], "%s,%s,%s_name=%s ", saved_section, influx_tags, saved_sub, escaped_resource);
    subended = 0;
    DEBUG fprintf(stderr, "ic_sub(\"%s\") count=%ld\n", resource, output_char);
}

void ic_subend(uint64_t stamp_nsec)
{
    ic_check( 4 + TIMESTAMP_STR_LEN);
    remove_ending_comma_if_any();
    if(stamp_nsec){
        output_char += sprintf(&output[output_char], "  %"PRIu64"\n", stamp_nsec);
    }else{
        output_char += sprintf(&output[output_char], "   \n");
    }
    subended = 1;
    DEBUG fprintf(stderr, "ic_subend()\n");
}

void ic_long(const char *name, long long value)
{
    ic_check( strlen(name) + 16 + 4 );

    output_char += ic_escape_str_cmap(ic_cmap_esc_fieldkey_tagkey_tagvalue, &output[output_char], output_size - output_char, name);

    output_char += snprintf(&output[output_char], output_size - output_char, "=%lldi,", value);
    DEBUG fprintf(stderr, "ic_long(\"%s\",%lld) count=%ld\n", name, value, output_char);
}

void ic_double(const char *name, double value)
{
    if (isnan(value) || isinf(value)) { /* not-a-number or infinity */
        DEBUG fprintf(stderr, "ic_double(%s,%.1f) - nan error\n", name, value);
    } else {
        ic_check( strlen(name) + 16 + 4 );

        output_char += ic_escape_str_cmap(ic_cmap_esc_fieldkey_tagkey_tagvalue, &output[output_char], output_size - output_char, name);

        output_char += snprintf(&output[output_char], output_size - output_char, "=%.3f,", value);

        DEBUG fprintf(stderr, "ic_double(\"%s\",%.1f) count=%ld\n", name, value, output_char);
    }
}

void ic_string(const char *name, char *value)
{
    ic_check( strlen(name) + strlen(value) + 4 );

    output_char += ic_escape_str_cmap(ic_cmap_esc_fieldkey_tagkey_tagvalue, &output[output_char], output_size - output_char, name);
    output[output_char++]='=';
    output[output_char++]='"';
    // Length limit 64KB.
    output_char += ic_escape_str_cmap(ic_cmap_esc_string_fieldvalue, &output[output_char], output_size - output_char, value);
    output[output_char++]='"';
    output[output_char++]=',';

    DEBUG fprintf(stderr, "ic_string(\"%s\",\"%s\") count=%ld\n", name, value, output_char);
}


static void ic_send_buffer(const char*output, size_t total){
    size_t sent = 0;
    ssize_t ret;
    if (debug == 2)
        fprintf(stderr, "output size=%zd output=\n<%s>\n", total, output);
    while (sent < total) {
        ret = write(sockfd, &output[sent], total - sent);
        DEBUG fprintf(stderr, "written=%zd bytes sent=%zd total=%zd\n", ret, sent, total);
        if (ret < 0) {
            fprintf(stderr, "warning: \"write body to sockfd failed.\" errno=%d\n", errno);
            break;
        }
        sent = sent + ret;
    }
}

static inline char ic_html_isSuccessful(int code)    { return (code >= 200 && code < 300); }
static inline char ic_html_isClientError(int code)   { return (code >= 400 && code < 500); }

static int ic_get_response(void){
    char result[1024]={0};
    ssize_t ret;
    int code=0;
    int i;

    if ((ret = read(sockfd, result, sizeof(result))) > 0) {
        result[ret] = 0;
        sscanf(result, "HTTP/1.1 %d", &code);
        if (debug > 0 || ! ic_html_isSuccessful(code) ){
            fprintf(stderr, "received bytes=%zd data=<%s>\n", ret, result);
        }
        for (i = 13; i < sizeof(result); i++){
            if (result[i] == '\r'){
                result[i] = 0;
            }
        }
        if (debug == 2){
            fprintf(stderr, "http-code=%d text=%s [204=Success]\n", code, &result[13]);
        }
        if ( ! ic_html_isSuccessful(code) ){
            fprintf(stderr, "code %d -->%s<--\n", code, result);
        }
        //if(ic_html_isClientError(code)){
        //    abort();
        //    exit(-1);
        //}
    }
    return code;
}

static int ic_post(const char*path, const char*content, size_t content_length, unsigned include_db, const char*add_header){
    char buffer[1024 * 8];
    int  ret=-1;

    if (influx_port) {
        if (create_socket() == 1) {
            snprintf(buffer, sizeof(buffer), "POST %s?db=%s&u=%s&p=%s HTTP/1.1\r\nHost: %s:%ld\r\nContent-Length: %zd\r\n%s\r\n",
                    path, include_db?influx_database:"",
                    influx_username, influx_password, influx_hostname, influx_port, content_length, add_header?add_header:NULL);
            DEBUG fprintf(stderr, "buffer size=%ld\nbuffer=<%s>\n", strlen(buffer), buffer);
            if ((ret = write(sockfd, buffer, strlen(buffer))) != strlen(buffer)) {
                    fprintf(stderr, "warning: \"write post to sockfd failed.\" errno=%d\n", errno);
            }

            if(content_length){
                ic_send_buffer(content, content_length);
            }
            ret = ic_get_response();
            
            close(sockfd);
            sockfd = -1;

        } else {
            DEBUG fprintf(stderr, "socket create failed\n");
        }
    } else error("influx port is not set, bailing out");

    return ret;
}

int ic_push()
{
    int ret=0;

    if (output_char == 0)       /* nothing to send so skip this operation */
        return ret;
    
    DEBUG fprintf(stderr, "ic_push() size=%ld\n", output_char);
    ret = ic_post("/write", output, output_char, 1, "");
    DEBUG fprintf(stderr, "ic_push complete\n");

    output[0] = 0;
    output_char = 0;
    return ret ;
}

int  ic_query(const char * query_str, unsigned include_db) 
{
    int ret=0;

    char buffer[1024 * 8];
    size_t pos=0;
    DEBUG fprintf(stderr, "%s(%s) size=%ld\n", __func__, query_str, strlen(query_str));

    buffer[pos++]='q';
    buffer[pos++]='=';
    pos += ic_urlencode(buffer + pos, sizeof(buffer) - pos, query_str);
    buffer[pos++]='\n';
    buffer[pos++]='\r';
    buffer[pos]='\0';

    ret = ic_post("/query", buffer, pos, 0, "Content-Type: application/x-www-form-urlencoded\r\n");
    DEBUG fprintf(stderr, "%s complete\n", __func__);
    
    return ret;
}

int  ic_create_db(void){
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "CREATE DATABASE \"%s\"", influx_database);
    return ic_query(buffer, 0) == 200? 0 : -1;
}

int  ic_drop_db(void){
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "DROP DATABASE \"%s\"", influx_database);
    return ic_query(buffer, 0) == 200? 0 : -1;
}

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

    ic_debug(2); /* maximum output */

    if(argc > 1){
        ic_influx_url(argv[1]);
    }else{
        ic_influx_database("silver2", 8086, "ic");
        ic_influx_userpw("nigel", "secret");
    }

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
        ic_measureend(0);

/* Measure with a single subsection - could be more */
 
        ic_measure("disks");
        for( i = 0; i<3; i++) {
            sprintf(buf, "sda%d",i);
            ic_sub(buf);
            ic_long("reads",    (long long)(i * RANGE(1,30)));
            ic_double("writes", (double)   (i * 3.142 * RANGE(1,30)));
            ic_subend(0);
        }
        ic_measureend(0);

/* Send all data in one packet to InfluxDB */

        ic_push();

/* Wait until we need to capture the data again */

        sleep(5);       /* Typically, this would be 60 seconds */
    }
    exit(0);
}
