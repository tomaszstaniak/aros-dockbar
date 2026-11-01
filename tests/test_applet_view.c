#include "applets/view.h"
#include "applets/snapshot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 struct DockAppletView v={0};
 const char *json="{\"schema\":1,\"expanded\":{\"component\":\"cards\",\"title\":\"location\",\"value\":\"temperature\",\"subtitle\":\"description\",\"strip\":\"hourly\",\"list\":\"daily\"}}";
 assert(dock_applet_view_parse(json,strlen(json),&v));
 assert(!strcmp(v.title,"location")&&!strcmp(v.strip,"hourly"));
 struct DockAppletView old=v;
 assert(!dock_applet_view_parse("{}",2,&v));assert(!memcmp(&v,&old,sizeof v));
 const char *bad="{\"schema\":1,\"expanded\":{\"component\":\"cards\",\"title\":\"x\",\"value\":\"x\",\"metrics\":[{},{},{}]}}";
 assert(!dock_applet_view_parse(bad,strlen(bad),&v));
 struct DockAppletSnapshot snapshot={0};
 strcpy(snapshot.icon,"cloudy");snapshot.field_count=1;
 strcpy(snapshot.fields[0].key,"temperature");strcpy(snapshot.fields[0].text,"16 C");
 strcpy(v.value,"temperature");
 uint64_t key=dock_applet_view_signature(&snapshot,&v,0);
 uint64_t compact=dock_applet_compact_signature(&snapshot,"temperature");
 strcpy(snapshot.icon,"cloudy-night");
 assert(key!=dock_applet_view_signature(&snapshot,&v,0));
 assert(compact!=dock_applet_compact_signature(&snapshot,"temperature"));
 strcpy(snapshot.icon,"cloudy");
 snapshot.updated=100;snapshot.generation=99;snapshot.status=DA_STALE;strcpy(snapshot.message,"Offline");
 assert(key==dock_applet_view_signature(&snapshot,&v,0));
 strcpy(snapshot.fields[0].text,"21 C");assert(key!=dock_applet_view_signature(&snapshot,&v,0));
 snapshot.row_count=7;
 for(int i=0;i<7;i++){strcpy(snapshot.rows[i].section,"hourly");strcpy(snapshot.rows[i].caption,"15:00");strcpy(snapshot.rows[i].value,"16 C");}
 key=dock_applet_view_signature(&snapshot,&v,0);
 strcpy(snapshot.rows[6].value,"20 C");assert(key==dock_applet_view_signature(&snapshot,&v,0));
 assert(key!=dock_applet_view_signature(&snapshot,&v,6));
 strcpy(snapshot.rows[0].value,"20 C");assert(key!=dock_applet_view_signature(&snapshot,&v,0));
 int x,y;
 dock_applet_anchor(1024,768,20,464,520,900,714,100,46,0,&x,&y);
 assert(x==552&&y==186);
 dock_applet_anchor(800,600,20,464,520,0,25,100,46,0,&x,&y);
 assert(x==8&&y==72);
 dock_applet_anchor(1024,768,20,464,520,980,200,40,60,1,&x,&y);
 assert(x==508&&y==28);
 puts("applet cards: metadata, failed-parse ownership and anchored screen bounds passed");return 0;
}
