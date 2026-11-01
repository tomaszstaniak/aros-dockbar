#include "model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
 assert(argc==2); const char *file=argv[1];
 struct DockModel m={0}; char out[DOCK_PATH];
 assert(dock_normalize_path("SYS:Tools/My App.INFO",out,sizeof out));
 assert(!strcmp(out,"SYS:Tools/My App"));
 assert(!dock_normalize_path("\n",out,sizeof out));
 assert(!dock_normalize_path(".info",out,sizeof out));
 assert(dock_pin(&m,"SYS:Tools/My App.info")==1);
 assert(m.count==1 && !strcmp(m.items[0].label,"My App"));
 assert(dock_pin(&m,"sys:tools/my app")==0 && m.count==1);
 assert(dock_pin(&m,"SYS:")==1 && !strcmp(m.items[1].label,"SYS:"));
 assert(dock_pin(&m,"SYS:bad\nline")==-1 && m.count==2);
 char longpath[DOCK_PATH+2];memset(longpath,'a',sizeof longpath);longpath[sizeof longpath-1]=0;
 assert(dock_pin(&m,longpath)==-1 && m.count==2);
 assert(dock_save_pins(&m,file));
 struct DockModel n={0}; assert(dock_load_pins(&n,file));
 assert(n.count==m.count && !strcmp(n.items[0].path,m.items[0].path));
 assert(dock_pin(&m,"SYS:One More")==1);
 assert(dock_save_pins(&m,file));
 memset(&n,0,sizeof n);assert(dock_load_pins(&n,file) && n.count==m.count);
 for(int i=m.count;i<DOCK_MAX_ITEMS;i++){char p[40];snprintf(p,sizeof p,"SYS:App%d",i);assert(dock_pin(&m,p)==1);}
 assert(dock_pin(&m,"SYS:Overflow")==-1 && m.count==DOCK_MAX_ITEMS);
 assert(dock_hit(&m,0,15,10,48)==-1);
 char old[1024];snprintf(old,sizeof old,"%s.old",file);
 assert(!rename(file,old));memset(&n,0,sizeof n);
 assert(dock_load_pins(&n,file)==-1 && n.count==3);
 assert(!rename(old,file));
 FILE *broken=fopen(file,"wb");assert(broken);fputs("SYS:Good\nSYS:bad\tname\n",broken);fclose(broken);
 memset(&n,0,sizeof n);assert(dock_load_pins(&n,file)==-1 && n.count==1);
 puts("PASS model: path normalization, duplicate drops, limits and persistence");
 return 0;
}
