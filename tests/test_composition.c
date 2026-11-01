#include "model.h"
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>

static struct DockModel m,before,loaded;
static void reset(void) { memset(&m,0,sizeof m); }
static void put(const char *file,const char *text) {
 FILE *f=fopen(file,"wb");assert(f);assert(fputs(text,f)>=0);assert(!fclose(f));
}
static void moves(void) {
 reset();assert(dock_add(&m,0,"SYS:A",0)==1);assert(dock_add(&m,0,"SYS:B",1)==1);
 assert(dock_add(&m,0,"SYS:C",2)==1);
 assert(dock_move(&m,0,0,0,3)==1);
 assert(!strcmp(m.items[0].path,"SYS:B") && !strcmp(m.items[2].path,"SYS:A"));
 before=m;assert(dock_move(&m,0,1,0,1)==0);assert(!memcmp(&m,&before,sizeof m));
 assert(dock_move(&m,0,1,0,2)==0);assert(!memcmp(&m,&before,sizeof m));
 assert(dock_move(&m,0,3,0,0)==-1);assert(!memcmp(&m,&before,sizeof m));
 assert(dock_add(&m,0,"sys:b.INFO",3)==0);assert(!memcmp(&m,&before,sizeof m));
 assert(dock_item_x(&m,0,48)==20);assert(dock_width(&m,48)==28+3*58);
 assert(dock_hit(&m,19,10,10,48)==-1 && dock_hit(&m,20,10,10,48)==0);
}
static void groups(void) {
 reset();assert(dock_add(&m,0,"SYS:A",0)==1);assert(dock_add(&m,0,"SYS:B",1)==1);
 assert(dock_add(&m,0,"SYS:C",2)==1);
 assert(dock_group_pair(&m,0,0,1)==1);
 assert(m.count==2 && m.items[0].kind==DOCK_GROUP && m.items[0].group==1);
 int n=0;struct DockItem *items=dock_items(&m,1,&n);
 assert(items && n==2 && !strcmp(items[0].path,"SYS:B") && !strcmp(items[1].path,"SYS:A"));
 before=m;assert(dock_move(&m,0,0,1,0)==-1);assert(!memcmp(&m,&before,sizeof m));
 assert(dock_group_pair(&m,1,0,0)==-1);assert(!memcmp(&m,&before,sizeof m));
 assert(dock_add(&m,0,"SYS:A",2)==1);
 before=m;assert(dock_move(&m,0,2,1,2)==0);assert(!memcmp(&m,&before,sizeof m));
 assert(dock_move(&m,1,0,0,1)==1);
 assert(m.count==4 && m.groups[0].count==1 && !strcmp(m.items[1].path,"SYS:B"));
 assert(dock_remove(&m,1,0)==1 && m.groups[0].used && m.groups[0].count==0);
 assert(dock_remove(&m,0,0)==1 && !m.groups[0].used && m.count==3);
 assert(!dock_items(&m,1,&n));
 assert(dock_group_pair(&m,0,1,0)==1);
 assert(m.items[0].kind==DOCK_GROUP && m.groups[0].count==2);
 assert(dock_group_pair(&m,1,0,1)==1 && m.groups[1].used && m.groups[0].count==1);
 assert(dock_move(&m,1,0,2,1)==1 && m.groups[0].count==0 && m.groups[1].count==3);
 assert(!strcmp(m.groups[1].items[1].path,"SYS:C"));
 before=m;assert(dock_remove(&m,2,3)==-1 && !memcmp(&m,&before,sizeof m));
}
static void limits(void) {
 reset();
 for(int i=0;i<DOCK_MAX_ITEMS;i++){char path[32];snprintf(path,sizeof path,"SYS:App%d",i);assert(dock_add(&m,0,path,m.count)==1);}
 before=m;assert(dock_add(&m,0,"SYS:Overflow",0)==-1);assert(!memcmp(&m,&before,sizeof m));
 assert(dock_group_pair(&m,0,1,0)==1);assert(dock_add(&m,1,"SYS:Child",2)==1);
 for(int i=m.groups[0].count;i<DOCK_MAX_ITEMS;i++){char path[32];snprintf(path,sizeof path,"SYS:Child%d",i);assert(dock_add(&m,1,path,i)==1);}
 before=m;assert(dock_move(&m,0,1,1,0)==-1);assert(!memcmp(&m,&before,sizeof m));
 reset();
 for(int i=0;i<DOCK_MAX_GROUPS;i++){
  char a[32],b[32];snprintf(a,sizeof a,"SYS:A%d",i);snprintf(b,sizeof b,"SYS:B%d",i);
  assert(dock_add(&m,0,a,m.count)==1);assert(dock_add(&m,0,b,m.count)==1);
  assert(dock_group_pair(&m,0,m.count-1,m.count-2)==1);
 }
 assert(dock_add(&m,0,"SYS:ExtraA",m.count)==1);assert(dock_add(&m,0,"SYS:ExtraB",m.count)==1);
 before=m;assert(dock_group_pair(&m,0,m.count-1,m.count-2)==-1);assert(!memcmp(&m,&before,sizeof m));
}
static void position(void) {
 struct DockPosition p={1,-99,INT_MAX};dock_clamp_position(&p,800,600,300,90);
 assert(p.valid && p.x==0 && p.y==510);
 p.x=999;p.y=-1;dock_clamp_position(&p,800,600,300,90);assert(p.x==500 && p.y==20);
 p.x=99;p.y=99;dock_clamp_position(&p,2,2,300,90);assert(p.x==0 && p.y==20);
 p.x=INT_MAX;p.y=INT_MAX;dock_clamp_position(&p,INT_MIN,INT_MIN,INT_MAX,INT_MAX);assert(p.x==0 && p.y==20);
}
static void config(const char *file) {
 char old[DOCK_PATH+8];snprintf(old,sizeof old,"%s.old",file);remove(file);remove(old);
 struct DockPosition p={1,170,411},q={0,12,99},original=q;
 reset();assert(dock_add(&m,0,"SYS:My App.info",0)==1);assert(dock_add(&m,0,"SYS:Other App",1)==1);
 assert(dock_group_pair(&m,0,1,0)==1);strcpy(m.items[0].label,"  My tools UTF-8 \303\263  ");
 strcpy(m.items[0].icon,"SYS:Pictures/Custom Icon.info");strcpy(m.groups[0].items[0].icon,"SYS:Custom Icon.info");
 assert(dock_add(&m,0,"SYS:Tools",1)==1);m.items[1].kind=DOCK_DRAWER;
 assert(dock_config_save(&m,&p,file));memset(&loaded,0,sizeof loaded);
 assert(dock_config_load(&loaded,&q,file)==1 && !memcmp(&m,&loaded,sizeof m));
 assert(!memcmp(&p,&q,sizeof p));
 char temporary[DOCK_PATH+8];snprintf(temporary,sizeof temporary,"%s.new",file);
 remove(temporary);assert(!mkdir(temporary,0700));
 assert(!dock_config_save(&m,&p,file));assert(!remove(temporary));
 memset(&loaded,0,sizeof loaded);assert(dock_config_load(&loaded,&q,file)==1 && !memcmp(&m,&loaded,sizeof m));
 assert(dock_config_save(&m,&p,file));assert(!rename(file,old));memset(&loaded,0,sizeof loaded);
 assert(dock_config_load(&loaded,&q,file)==-1 && !memcmp(&m,&loaded,sizeof m));
 assert(!dock_config_save(&m,&p,file));assert(!rename(old,file));
 before=loaded;original=q;
 put(file,"DOCKBAR\t2\nEND\n");assert(dock_config_load(&loaded,&q,file)==-1);
 assert(!dock_config_save(&m,&p,file));
 assert(!memcmp(&loaded,&before,sizeof loaded) && !memcmp(&q,&original,sizeof q));
 put(file,"DOCKBAR\t1\nPOSITION\t1\t3\t4\nROOT\t0\n");assert(dock_config_load(&loaded,&q,file)==-1);
 assert(!memcmp(&loaded,&before,sizeof loaded));
 put(file,"DOCKBAR\t1\nPOSITION\t1\t3\t4\nROOT\t1\nITEM\t1\t1\t\tGroup\t\nEND\n");
 assert(dock_config_load(&loaded,&q,file)==-1 && !memcmp(&loaded,&before,sizeof loaded));
 put(file,"DOCKBAR\t1\nPOSITION\t1\t3\t4\nROOT\t0\nEND\ntrailing\n");
 assert(dock_config_load(&loaded,&q,file)==-1 && !memcmp(&loaded,&before,sizeof loaded));
 const char *bad[]={
  "DOCKBAR\t1\nPOSITION\t2\t0\t0\nROOT\t0\nEND\n",
  "DOCKBAR\t1\nPOSITION\t1\t999999999999999999999\t0\nROOT\t0\nEND\n",
  "DOCKBAR\t1\nPOSITION\t1\t0\t0\nROOT\t25\nEND\n",
  "DOCKBAR\t1\nPOSITION\t1\t0\t0\nROOT\t0\nGROUP\t1\t0\nEND\n",
  "DOCKBAR\t1\nPOSITION\t1\t0\t0\nROOT\t1\nITEM\t1\t1\t\tGroup\t\nGROUP\t1\t1\nITEM\t1\t2\t\tNested\t\nEND\n",
  "DOCKBAR\t1\nPOSITION\t1\t0\t0\nROOT\t2\nITEM\t0\t0\tSYS:A\tA\t\nITEM\t0\t0\tsys:a\tA\t\nEND\n",
  "DOCKBAR\t1\nPOSITION\t1\t0\t0\nROOT\t1\nITEM\t5\t0\tSYS:A\tA\t\nEND\n",
  "DOCKBAR\t1\nPOSITION\t1\t0\t0\nROOT\t1\nITEM\t1\t1\tSYS:Bad\tGroup\t\nGROUP\t1\t0\nEND\n"
 };
 for(size_t i=0;i<sizeof bad/sizeof bad[0];i++) {
  put(file,bad[i]);assert(dock_config_load(&loaded,&q,file)==-1);
  assert(!memcmp(&loaded,&before,sizeof loaded) && !memcmp(&q,&original,sizeof q));
  assert(!dock_config_save(&m,&p,file));
 }
 strcpy(m.items[0].label,"bad\tlabel");assert(!dock_config_save(&m,&p,file));
 remove(file);assert(!mkdir(file,0700));assert(!dock_config_save(&before,&p,file));
 assert(dock_config_load(&loaded,&q,file)==-1 && !memcmp(&loaded,&before,sizeof loaded));
 assert(!remove(file));assert(dock_config_load(&loaded,&q,file)==0 && !memcmp(&loaded,&before,sizeof loaded));
}
int main(int argc,char **argv) {
 assert(argc==2);moves();groups();limits();position();config(argv[1]);
 puts("PASS composition: groups, moves, limits, atomic mutations, positions and transactional config");return 0;
}
