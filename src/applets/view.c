#include "view.h"
#include "json.h"
#include "snapshot.h"
#include <stdlib.h>
#include <string.h>
static int field(const struct DockJson *j,int at,const char *key,char *out,size_t n,int required){
 int token=dock_json_get(j,at,key);
 return token<0?!required:dock_json_string(j,token,out,n);
}
int dock_applet_view_parse(const char *s,size_t n,struct DockAppletView *out){
 struct DockJson *j=malloc(sizeof *j);struct DockAppletView v={0};int ok=0,version;
 if(!j)return 0;
 if(!dock_json_parse(j,s,n)||!dock_json_int(j,dock_json_get(j,0,"schema"),&version)||version!=1)goto done;
 int a=dock_json_get(j,0,"expanded");
 if(!dock_json_is(j,dock_json_get(j,a,"component"),"cards"))goto done;
 if(!field(j,a,"title",v.title,sizeof v.title,1)||!field(j,a,"value",v.value,sizeof v.value,1)||
    !field(j,a,"subtitle",v.subtitle,sizeof v.subtitle,0)||!field(j,a,"range",v.range,sizeof v.range,0)||
    !field(j,a,"strip",v.strip,sizeof v.strip,0)||!field(j,a,"list",v.list,sizeof v.list,0)||
    !field(j,a,"stripTitle",v.strip_title,sizeof v.strip_title,0)||!field(j,a,"listTitle",v.list_title,sizeof v.list_title,0))goto done;
 int m=dock_json_get(j,a,"metrics");
 if(m>=0){if(j->token[m].type!=DJ_ARRAY||j->token[m].count>2)goto done;
  for(int i=0;i<j->token[m].count;i++){
   int t=dock_json_at(j,m,i);
   if(!field(j,t,"field",v.metric[i],sizeof v.metric[i],1)||!field(j,t,"label",v.metric_label[i],sizeof v.metric_label[i],1))goto done;
  }
 }
 *out=v;ok=1;
 done:free(j);return ok;
}
static int clamp(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
void dock_applet_anchor(int sw,int sh,int bar,int w,int h,int ax,int ay,int aw,int ah,int vertical,int *x,int *y){
 if(vertical){*x=ax+aw+8;if(*x+w>sw-8)*x=ax-w-8;*y=ay+ah/2-h/2;}
 else{*x=ax+aw/2-w/2;*y=ay-h-8;if(*y<bar+8)*y=ay+ah+8;}
 *x=clamp(*x,8,sw-w-8);*y=clamp(*y,bar+8,sh-h-8);
}

static uint64_t hash_text(uint64_t hash,const char *s){
 for(;*s;s++){hash^=(unsigned char)*s;hash*=1099511628211ULL;}
 hash^=0;return hash*1099511628211ULL;
}
uint64_t dock_applet_compact_signature(const struct DockAppletSnapshot *s,const char *primary){
 return hash_text(hash_text(14695981039346656037ULL,s->icon),dock_applet_field(s,primary));
}
uint64_t dock_applet_view_signature(const struct DockAppletSnapshot *s,const struct DockAppletView *v,int page){
 uint64_t hash=hash_text(14695981039346656037ULL,s->icon);
 const char *keys[]={v->title,v->value,v->subtitle,v->range,v->metric[0],v->metric[1]};
 for(unsigned i=0;i<sizeof keys/sizeof *keys;i++)hash=hash_text(hash,dock_applet_field(s,keys[i]));
 for(int section=0;section<2;section++){
  const char *key=section?v->list:v->strip;int seen=0,start=section?0:page,limit=section?5:6;
  if(!key[0])continue;
  for(int i=0;i<s->row_count;i++)if(!strcmp(key,s->rows[i].section)){
   if(seen++<start)continue;
   if(seen>start+limit)break;
   const struct DockAppletRow *r=&s->rows[i];
   hash=hash_text(hash,r->caption[0]?r->caption:r->label);hash=hash_text(hash,r->value);
   hash=hash_text(hash,r->detail);hash=hash_text(hash,r->icon);
  }
 }
 return hash;
}
