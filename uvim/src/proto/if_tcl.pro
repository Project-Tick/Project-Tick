/* if_tcl.c */
void mnv_tcl_init(char *arg);
int tcl_enabled(int verbose);
void mnv_tcl_finalize(void);
void tcl_end(void);
void ex_tcl(exarg_T *eap);
void ex_tclfile(exarg_T *eap);
void ex_tcldo(exarg_T *eap);
void tcl_buffer_free(buf_T *buf);
void tcl_window_free(win_T *win);
/* mnv: set ft=c : */
