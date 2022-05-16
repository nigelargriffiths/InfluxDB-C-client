/*
 * Influx C (ic) client for data capture header file
 * Developer: Nigel Griffiths.
 * (C) Copyright 2021 Nigel Griffiths
 */
 void ic_influx_url(const char * influx_db);
 void ic_influx_database(const char *host, long port, const char *db);
 void ic_influx_userpw(const char *user, const char *pw);
 void ic_tags(const char *tags);
 void ic_tags_escaped(const char*first, ...);

 void ic_measure(const char *section);
 void ic_measureend(uint64_t);

 void ic_sub(const char *sub_name);
 void ic_subend(uint64_t);
 
 int  ic_create_db(void);
 int  ic_drop_db(void);

 void ic_long(const char *name, long long value);
 void ic_double(const char *name, double value);
 void ic_string(const char *name, char *value);

 int  ic_push();
 void ic_debug(int level);

