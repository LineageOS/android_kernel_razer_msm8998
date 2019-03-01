#ifndef __FIH_HWID_H
#define __FIH_HWID_H

void fih_hwid_setup(void);
void fih_hwid_print(struct st_hwid_table *tb);
void fih_hwid_read(struct st_hwid_table *tb);
int fih_hwid_fetch(int idx);

#endif /* __FIH_HWID_H */
