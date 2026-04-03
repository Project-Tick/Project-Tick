/* mnvinfo.c */
int get_mnvinfo_parameter(int type);
int buf_compare(const void *s1, const void *s2);
void check_marks_read(void);
int read_mnvinfo(char_u *file, int flags);
void write_mnvinfo(char_u *file, int forceit);
void ex_mnvinfo(exarg_T *eap);
/* mnv: set ft=c : */
