/* Finite protocol fixture for native transition recordings. Not a weather source. */
#include "applets/json.h"
#include "applets/snapshot.h"
#ifdef __AROS__
#include <proto/dos.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static void field(struct DockAppletSnapshot *s,const char *key,const char *value){
 struct DockAppletField *f=&s->fields[s->field_count++];snprintf(f->key,sizeof f->key,"%s",key);snprintf(f->text,sizeof f->text,"%s",value);
}
int main(int argc,char **argv){
 if(argc!=2)return 5;
 size_t n;char *input=dock_applet_read(argv[1],65536,&n);struct DockJson *j=malloc(sizeof *j);
 struct DockAppletSnapshot *s=calloc(1,sizeof *s);int generation,result=5;
 if(!input||!j||!s||!dock_json_parse(j,input,n))goto done;
 if(!dock_json_string(j,dock_json_get(j,0,"package"),s->package,sizeof s->package)||
    !dock_json_string(j,dock_json_get(j,0,"instance"),s->instance,sizeof s->instance)||
    !dock_json_string(j,dock_json_get(j,0,"session"),s->session,sizeof s->session)||
    !dock_json_int(j,dock_json_get(j,0,"generation"),&generation))goto done;
#ifdef __AROS__
 Delay(10);
#endif
 s->generation=generation;s->status=DA_READY;s->updated=s->source_time=(double)time(NULL);
 int warm=!(generation%2);const char *icon=warm?"clear-day":"cloudy";
 strcpy(s->title,"Transition demo");strcpy(s->fingerprint,"controlled-test");strcpy(s->icon,icon);
 strcpy(s->attribution,"Controlled demo data");
 field(s,"location","Transition demo");field(s,"temperature",warm?"21 C":"16 C");
 field(s,"description",warm?"Clear sky":"Cloudy");field(s,"range",warm?"Low 12   High 21 C":"Low 7   High 16 C");
 field(s,"feelsLike",warm?"20 C":"14 C");field(s,"wind",warm?"5 km/h":"8 km/h");
 const char *days[]={"Today","Fri","Sat","Sun","Mon"};
 for(int i=0;i<11;i++){
  struct DockAppletRow *r=&s->rows[s->row_count++];strcpy(r->icon,icon);
  strcpy(r->section,i<6?"hourly":"daily");
  if(i<6)snprintf(r->caption,sizeof r->caption,"%02d:00",15+i);else strcpy(r->caption,days[i-6]);
  strcpy(r->label,r->caption);strcpy(r->value,i<6?(warm?"21 C":"16 C"):(warm?"12 / 21 C":"7 / 16 C"));strcpy(r->detail,warm?"0%":"20%");
 }
 char tmp[1024],out[1024];
 if(snprintf(tmp,sizeof tmp,"%s.tmp",argv[1])>=(int)sizeof tmp||snprintf(out,sizeof out,"%s.done",argv[1])>=(int)sizeof out)goto done;
 FILE *f=fopen(tmp,"wb");if(!f)goto done;
 int ok=dock_applet_snapshot_write(f,s);if(fclose(f))ok=0;
 if(ok&&!rename(tmp,out))result=0;
 done:free(input);free(j);free(s);return result;
}
