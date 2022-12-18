/*
 * Influx C (ic) client for data capture header file
 * Developer: Nigel Griffiths.
 * (C) Copyright 2021 Nigel Griffiths
 */

#ifdef __cplusplus
extern "C"
{
#endif


 void ic_influx_database(const char *host, long port, const char *db);
 void ic_influx_userpw(const char *user, const char *pw);
 void ic_tags(const char *tags);

 void ic_measure(const char *section);
 void ic_measureend();

 void ic_sub(const char *sub_name);
 void ic_subend();

 void ic_long(const char *name, long long value);
 void ic_double(const char *name, double value);
 void ic_string(const char *name, const char *value);

 void ic_push();
 void ic_debug(int level);

#ifdef __cplusplus
}
#endif

